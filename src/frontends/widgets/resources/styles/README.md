# Application styles

`edit_atlas.qss` contains visual Qt widget rules, including selectors based on
widget object names.

`application_style.cpp` retains non-visual application behavior: the Fusion
base style, disabled widget animations, minimum interface font size, and the
semantic `QPalette` color roles. Individual widgets continue to own their
layout, content, and interaction behavior.
