"""Dump a native Windows file dialog's UIA tree from an isolated process."""

from __future__ import annotations

import argparse
from pathlib import Path
import traceback
from typing import Any


_PATTERNS = (
    "iface_expand_collapse",
    "iface_invoke",
    "iface_legacy_iaccessible",
    "iface_range_value",
    "iface_selection",
    "iface_selection_item",
    "iface_text",
    "iface_toggle",
    "iface_value",
)


def _property(node: Any, name: str, default: str = "") -> str:
    try:
        return str(getattr(node.element_info, name) or default)
    except Exception:
        return default


def _state(node: Any, method: str) -> str:
    try:
        return str(bool(getattr(node, method)()))
    except Exception as error:
        return f"error:{type(error).__name__}"


def _patterns(node: Any) -> str:
    available: list[str] = []
    for name in _PATTERNS:
        try:
            if getattr(node, name) is not None:
                available.append(name.removeprefix("iface_"))
        except Exception:
            continue
    return ",".join(available)


def _dump_node(node: Any, lines: list[str], depth: int) -> None:
    lines.append(
        f"{'  ' * depth}{_property(node, 'control_type', '?')} "
        f"name={_property(node, 'name')!r} "
        f"automation_id={_property(node, 'automation_id')!r} "
        f"class_name={_property(node, 'class_name')!r} "
        f"visible={_state(node, 'is_visible')} "
        f"enabled={_state(node, 'is_enabled')} "
        f"focused={_state(node, 'has_keyboard_focus')} "
        f"patterns={_patterns(node)!r}"
    )
    try:
        children = node.children()
    except Exception as error:
        lines.append(
            f"{'  ' * (depth + 1)}children-error={type(error).__name__}: {error}"
        )
        return
    for child in children:
        _dump_node(child, lines, depth + 1)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--handle", required=True, type=int)
    parser.add_argument("--output", required=True, type=Path)
    arguments = parser.parse_args()
    arguments.output.parent.mkdir(parents=True, exist_ok=True)

    try:
        from pywinauto import Desktop

        dialog = Desktop(backend="uia").window(
            handle=arguments.handle
        ).wrapper_object()
        lines: list[str] = []
        _dump_node(dialog, lines, 0)
        arguments.output.write_text("\n".join(lines) + "\n", encoding="utf-8")
        return 0
    except Exception:
        arguments.output.write_text(traceback.format_exc(), encoding="utf-8")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
