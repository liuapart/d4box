"""Mac-runnable regression for the actual shipped legacy workflow data."""
from configparser import ConfigParser
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
CONFIG = ROOT / "app/ahk/config/cube-workflows.ini"


class BuiltinWorkflowTests(unittest.TestCase):
    def setUp(self):
        parser = ConfigParser(interpolation=None)
        parser.read(CONFIG, encoding="ascii")
        self.profile = parser["Workflow.inventory_transmute"]
        self.steps = [self.profile[f"Step{i}"].split("|")
                      for i in range(1, self.profile.getint("StepCount") + 1)]

    def route(self, quality):
        return [step for step in self.steps if step[2] in (quality, "any")]

    def test_defaults_preserve_existing_inventory_behavior(self):
        self.assertEqual(self.profile["Source"], "inventory")
        self.assertEqual(self.profile["Quality"], "red")
        self.assertEqual(self.profile["Space"], "screen")
        self.assertEqual(self.profile.getint("MaxItems"), 33)
        self.assertEqual(self.profile.getint("LegacyYellow"), 1)

    def test_legendary_action_order_and_coordinates(self):
        route = self.route("red")
        self.assertEqual([s[1] for s in route], ["select", "execute", "confirm", "clear"])
        self.assertEqual([(float(s[3]), float(s[4])) for s in route],
                         [(0.445, 0.380), (0.168, 0.793), (0.432, 0.560), (0.168, 0.680)])
        self.assertEqual([int(s[5]) for s in route], [180, 450, 1000, 350])

    def test_rare_chain_retains_distinct_transmute_position(self):
        route = self.route("yellow")
        self.assertEqual([s[1] for s in route], ["select", "execute", "confirm"] * 3 + ["clear"])
        self.assertEqual([float(s[4]) for s in route if s[1] == "select"], [0.233, 0.720, 0.350])
        self.assertEqual(sum(s[1] == "clear" for s in route), 1)

    def test_default_data_schema_and_localized_labels(self):
        language = (ROOT / "app/ahk/utils/Lang.ahk").read_text()
        self.assertIn('"' + self.profile["NameKey"] + '"', language)
        for step in self.steps:
            self.assertEqual(len(step), 9)
            self.assertIn(f'"cube.step.{step[0]}"', language)
            self.assertIn(step[1], {"select", "execute", "confirm", "clear", "wait", "check"})
            self.assertIn(step[2], {"any", "red", "yellow", "blue"})
            for value in step[3:5]:
                self.assertTrue(0 <= float(value) <= 1)
            self.assertTrue(0 <= int(step[5]) <= 10000)

    def test_no_orphaned_steps_after_declared_count(self):
        numbered = {key for key in self.profile if key.startswith("step") and key[4:].isdigit()}
        self.assertEqual(numbered, {f"step{i}" for i in range(1, len(self.steps) + 1)})


if __name__ == "__main__":
    unittest.main()
