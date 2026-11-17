#!/usr/bin/env bash
# d4box 测试入口（迁自 D4KeyHelp）：把「加载期校验 → 冒烟测试 → 流程回归」串成一条命令。
#
#   bash tests/run.sh                  # 依次跑全部
#   bash tests/run.sh validate         # 只跑加载期校验（最快，改完立刻自检用）
#   bash tests/run.sh smoke            # 只跑冒烟测试
#
# 退出码：0 = 全部通过，1 = 有失败项，2 = 环境不满足（找不到解释器等）。
#
# 为什么这里不写裸的 `AutoHotkey64.exe tests/SmokeTest.ahk`：
#   1) 解释器不在 PATH 上，必须用绝对路径（本脚本会自动探测）；
#   2) MSYS/Git-Bash 会把 /validate 改写成 Windows 路径（变成 C:/Program Files/Git/validate），
#      AHK 收到的是一个不存在的脚本名而不是开关，于是脚本被真正执行、进程挂住不退出。
#      所以开关必须写成 //validate（双斜杠）。MSYS_NO_PATHCONV=1 也能挡住改写，但它同时会
#      把 /c/... 形式的脚本路径也一并破坏，所以本脚本统一用双斜杠写法。
#   3) 加载期报错时 AHK 默认弹对话框等用户点确定，非交互环境下会永久挂住；
#      对「语法错误」加 //ErrorStdOut 能把消息改发到 stderr，但对「调用不存在的函数」
#      这类加载期错误它仍会弹窗，所以本脚本再叠一层 timeout 兜底（见 AHK_TIMEOUT_SECS）。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT" || exit 2

STEP="${1:-all}"
shift || true

# ---------- 环境探测 ----------

find_ahk() {
    # 显式指定了 AHK 就只认它：写错了应当报错，而不是悄悄换一个解释器去跑
    if [ -n "${AHK:-}" ]; then
        if [ -f "${AHK}" ]; then printf '%s\n' "$AHK"; return 0; fi
        return 1
    fi
    local cand
    for cand in \
        "$REPO_ROOT/../../.runtime/autohotkey/v2/AutoHotkey64.exe" \
        "/c/Program Files/AutoHotkey/v2/AutoHotkey64.exe" \
        "/c/Program Files/AutoHotkey/v2/AutoHotkey.exe" \
        "/c/Program Files/AutoHotkey/AutoHotkey64.exe" \
        "$(command -v AutoHotkey64.exe 2>/dev/null || true)" \
        "$(command -v AutoHotkey.exe 2>/dev/null || true)"
    do
        if [ -n "$cand" ] && [ -f "$cand" ]; then printf '%s\n' "$cand"; return 0; fi
    done
    return 1
}

AHK="$(find_ahk || true)"
if [ -z "$AHK" ]; then
    echo "找不到 AutoHotkey v2 解释器。请安装，或用 AHK=/path/to/AutoHotkey64.exe 指定。" >&2
    exit 2
fi

# 给 AHK 调用加个超时兜底。正常情况下不该触发；触发说明脚本卡住了
# （最常见的原因就是弹了模态错误对话框在等用户点确定）。
# validate 正常只要约 0.1 秒，冒烟/流程测试正常 2-3 秒；这里取 30 秒很宽松。
# 注意：不同步骤的期望耗时差很多，所以允许用 RUN_TIMEOUT 单独收紧（见 step_validate）。
AHK_TIMEOUT_SECS=30
AHK_TIMEOUT=""
if command -v timeout >/dev/null 2>&1; then
    AHK_TIMEOUT="timeout $AHK_TIMEOUT_SECS"
fi

# 跑一次 AHK 并把输出/退出码放到全局，供各步骤使用。
# 超时被杀时退出码是 124，会被上层当作失败，而不是把整个 runner 拖住。
#
# 参数顺序很重要：AHK 的用法是 `AutoHotkey.exe [开关...] [脚本] [脚本参数...]`，
# 开关必须放在脚本**前面**。写在脚本后面会被当成「传给脚本的参数」而不是开关，
# 于是 //validate 静默失效、脚本被真的执行（D4Box.ahk 是主程序，会一直挂着）。
run_ahk() {  # run_ahk [开关...] <脚本>
    local to="${RUN_TIMEOUT:-$AHK_TIMEOUT}"
    AHK_OUT="$($to "$AHK" "$@" 2>&1)"
    AHK_RC=$?
    return 0
}

# ---------- 结果汇总 ----------

PASS=0
FAIL=0
FAILED_STEPS=()

report() {  # report <名称> <0=通过 1=失败> [详情]
    local name="$1" ok="$2" detail="${3:-}"
    if [ "$ok" -eq 0 ]; then
        printf '  [通过] %s%s\n' "$name" "${detail:+  ($detail)}"
        PASS=$((PASS + 1))
    else
        printf '  [失败] %s%s\n' "$name" "${detail:+  ($detail)}"
        FAIL=$((FAIL + 1))
        FAILED_STEPS+=("$name")
    fi
}

indent() { sed 's/^/        /'; }

# ---------- 1. 加载期校验 ----------

step_validate() {
    echo "== 1/3 加载期校验（语法 + #Include + 函数名是否定义）=="
    local f rc
    # validate 正常约 0.1 秒，收紧到 10 秒，出错时不必白等 30 秒
    local RUN_TIMEOUT
    if command -v timeout >/dev/null 2>&1; then RUN_TIMEOUT="timeout 10"; fi
    for f in D4Box.ahk tests/SmokeTest.ahk tests/CubeWorkflowTest.ahk tests/UpdateTest.ahk; do
        run_ahk //validate //ErrorStdOut "$f"
        rc=$AHK_RC
        if [ "$rc" -eq 0 ]; then
            report "$f" 0
        elif [ "$rc" -eq 124 ]; then
            # /validate 对「语法错误」和「#Include 找不到目标」会把消息发到 stderr 并正常退出，
            # 但对「调用不存在的函数」这类加载期错误会弹模态对话框，//ErrorStdOut 拦不住，
            # 进程就一直等用户点确定。这里靠上面的 timeout 兜底，并提示可能是这类弹窗。
            report "$f" 1 "超时 10s 未返回（很可能弹了错误对话框在等确认）"
            printf '%s\n' "$AHK_OUT" | grep -v '^$' | indent
        else
            report "$f" 1 "退出码 $rc"
            printf '%s\n' "$AHK_OUT" | grep -v '^$' | indent
        fi
    done
}

# ---------- 2. 冒烟测试 ----------

step_smoke() {
    echo "== 2/3 冒烟测试（加载全部模块 + 创建界面）=="
    local report_file="tests/smoke_result.txt"
    rm -f "$report_file"
    run_ahk tests/SmokeTest.ahk

    if [ ! -f "$report_file" ]; then
        if [ "$AHK_RC" -eq 124 ]; then
            report "冒烟测试" 1 "超时 ${AHK_TIMEOUT_SECS}s 未返回（很可能弹了错误对话框在等确认）"
        else
            report "冒烟测试" 1 "没有生成 $report_file（退出码 $AHK_RC）"
        fi
        return 1
    fi
    # SmokeTest 内部用 try/catch 兜住异常，所以进程退出码恒为 0，
    # 只能看结果文件里有没有「失败」「异常」字样。
    if grep -qE '失败|异常' "$report_file"; then
        report "冒烟测试" 1 "结果文件里有失败项"
        indent < "$report_file"
        return 1
    fi
    report "冒烟测试" 0
    indent < "$report_file"
    return 0
}

# ---------- 3. 流程回归 ----------


# Workflow tests use a fake adapter: no game input and no screenshot fixtures.
step_workflow() {
    echo "== Workflow/model regression =="
    local report_file="tests/cube_result.txt"
    rm -f "$report_file"
    run_ahk tests/CubeWorkflowTest.ahk
    if [ "$AHK_RC" -ne 0 ] || [ ! -f "$report_file" ]; then
        report "workflow" 1 "exit=$AHK_RC or missing report"
        [ -f "$report_file" ] && indent < "$report_file"
        return 1
    fi
    if grep -q '^FAIL ' "$report_file" || ! grep -q '^done$' "$report_file"; then
        report "workflow" 1 "regression failed or incomplete"
        indent < "$report_file"
        return 1
    fi
    report "workflow" 0
    indent < "$report_file"
}

# ---------- 调度 ----------

step_update() {
    local report_file="tests/update_result.txt"
    rm -f "$report_file"
    run_ahk //ErrorStdOut tests/UpdateTest.ahk
    if [ "$AHK_RC" -ne 0 ] || [ ! -f "$report_file" ] || grep -q '^FAIL ' "$report_file" || ! grep -q '^done$' "$report_file"; then
        report "update" 1 "exit=$AHK_RC or incomplete regression"
        printf '%s\n' "$AHK_OUT" | indent
        [ -f "$report_file" ] && indent < "$report_file"
        return 1
    fi
    report "update" 0
    indent < "$report_file"
}

echo "解释器: $AHK"
echo

case "$STEP" in
    validate) step_validate ;;
    smoke)    step_smoke ;;
    workflow) step_workflow ;;
    update)   step_update ;;
    all)      step_validate; step_smoke; step_workflow; step_update ;;
    *)
        echo "用法: bash tests/run.sh [validate|smoke|workflow|update|all]" >&2
        exit 2
        ;;
esac

echo
echo "== 汇总: 通过 $PASS 项，失败 $FAIL 项 =="
if [ "$FAIL" -gt 0 ]; then
    echo "失败项: ${FAILED_STEPS[*]:-}"
    exit 1
fi
exit 0
