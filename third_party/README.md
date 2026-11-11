# Third-party source

`third_party/` contains third-party source that is **tracked** in this
repository. `lib/` is different: it contains generated/downloaded dependencies
and is ignored by git.

## ImGuiFileDialog

`third_party/ImGuiFileDialog` is a vendored copy of ImGuiFileDialog v0.6.8
(MIT), with local changes for runtime UI translations. See its `LICENSE`,
`VERSION.txt` and `LOCAL_CHANGES.md`. The code is not QtreGBPaint code; it is
pinned here until the project decides to update or replace it.
