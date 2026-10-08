# Cramer's rule 2D explainer

Project memory for any coding agent. Read this before researching Cramer's rule or changing the visualizer. Update it in the same session when a decision or a researched fact should survive to the next machine.

This folder is the project. The git repo root is the parent `graphics` repo (`https://github.com/focusright/graphics.git`, branch `main`). Open this folder as the workspace so this file sits at the project root.

## Status

- Goal: a 2D Cramer's rule explainer/visualizer matching 3Blue1Brown, Essence of Linear Algebra, Chapter 12.
- Video: https://www.youtube.com/watch?v=jBsC34PxzoM
- Lesson text: https://www.3blue1brown.com/lessons/cramers-rule/
- Animation source (scene order and the real numbers): https://github.com/3b1b/videos/blob/master/_2018/cramer.py
- 3D is the same idea with signed volume. Leave it alone until the user asks.
- Implementation has not started. The user is still building understanding. Do not rewrite the page unless they ask.
- Research below was already done (video transcript, lesson page, manim source). Do not repeat that fetch unless the user asks a question these notes do not answer.

## What the explainer has to show

A linear system is a known transform applied to an unknown input, with a known output:

```
A [x, y]^T = v
```

Columns of `A` are where i-hat and j-hat land. The puzzle is which input lands on `v`. Equivalently, `v = x a1 + y a2`. Assume `det(A) ≠ 0` (unique input for every output). Zero determinant is mentioned, then set aside.

The argument, in the video's order:

1. Dot products are the tempting wrong idea. `x = input · i-hat` and `y = input · j-hat` in the input plane. Those dots do not survive a general linear map. Only orthonormal transforms (rotations / rigid motions) preserve them, and only then does "dot the output with the columns" solve the system. This is a side path.
2. A coordinate can instead be a signed area, and every signed area scales by the same factor, `det(A)`.
3. In the input plane, before `A`:
   - Parallelogram spanned by i-hat and the mystery input has signed area `y` (base 1, height `y`). Video teaches `y` first.
   - Parallelogram spanned by the mystery input and j-hat has signed area `x`.
   - Order matters: `y = det[i-hat | input]`, `x = det[input | j-hat]`. Negative coordinates are negative signed areas.
4. Apply `A`. i-hat goes to column 1, j-hat to column 2, the mystery input goes to `v`.
   - The y-parallelogram becomes the parallelogram of column 1 and `v`, area `det(A) * y`.
   - The x-parallelogram becomes the parallelogram of `v` and column 2, area `det(A) * x`.
5. Those output vectors are known from the system. Divide the new area by `det(A)`:

```
y = det[a1 | v] / det(A)
x = det[v | a2] / det(A)
```

Column replacement means: put the output vector into the slot the mystery input occupied. The ratio of areas is the invariant. The coordinate is recovered from output-space data alone.

One sentence: a coordinate is a signed area; the transform multiplies that area by `det(A)`; dividing the output area by `det(A)` returns the coordinate.

There is a second, algebraic proof: substitute `v = x a1 + y a2` into `det(v, a2)` and the `y` term vanishes because `det(a2, a2) = 0`. That proof is true. It is not the video. The video is input-space area, then uniform scaling, then an output-space area you can measure.

## Numbers already checked

The video uses two different 2×2 examples. Do not mix them.

Opening puzzle only (known matrix, mystery input). Not the sanity-check arithmetic:

```
A = |  3  2 |
    | -1  2 |
v = [-4, -2]
```

Geometric core and the spoken sanity check. Manim scenes `TransformingAreasYCoord` / `TransformingAreasXCoord`:

```
A = | 2  -1 |     det(A) = 2
    | 0   1 |
input  = [3, 2]
output = [4, 2]

x = det| 4  -1 | / 2 = (4*1 - (-1)*2) / 2 = 6/2 = 3
       | 2   1 |

y = det| 2   4 | / 2 = (2*2 - 4*0) / 2 = 4/2 = 2
       | 0   2 |
```

Direct check: `A [3, 2]^T = [4, 2]^T`.

## Existing file

`cramers_rule_2d_microscope.html` is a previous attempt. Keep it until a replacement exists.

Use from it:

- One self-contained HTML file.
- Three.js as an ES module via an import map.
- MathJax for the formulas.
- Dark page, a HUD of live numbers, draggable handles, a reset control.
- `det2(a, b) = a.x * b.y - a.y * b.x` as signed area.

Do not treat its pictures as the lesson. It draws three arrows `A0`, `A1`, and `C` on one plane and explains the algebraic filter (the other component "dies" because its determinant is zero). It uses a perspective camera, orbit controls, and extruded slabs whose height stands in for the determinant. It never shows the input plane, i-hat and j-hat as the coordinate-measuring basis, the mystery input, a morphing grid, or the same parallelogram before and after the transform.

The 2D explainer wants an orthographic view of the xy-plane, a grid that transforms, and a before/after of the two coordinate parallelograms. The MLS-MPM pages next to this repo (`codebase/three.js/threejs_mls_mpm_pages/`) already use `OrthographicCamera` on the xy-plane; that is the relevant Three.js pattern. The folder name `moller-trumbore` is unrelated. Leave it.

## When this file should change

Add a dated line under Session log when any of these happen: a teaching beat is confirmed or rejected, the example matrix changes, implementation starts, or a file is added or replaced. Keep the log short. Put durable facts in the sections above, not only in the log.

## Session log

- 2026-09-20: Read the video, the lesson page, and `_2018/cramer.py`. Recorded the 2D geometry and the two examples. No code changes. User asked to wait on implementation.
- 2026-10-08: Added this file so later sessions on other machines do not repeat that research.
