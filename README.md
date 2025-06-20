# waterfall-picker

This is a c (actually c++ on account of operator overloads) raylib program for the following sqlite3 database schema:

![schema](schema.svg)


[waterfall cad](https://github.com/joe-warren/opencascade-hs#readme) has no
feature to graphically pick points on a model. A separate quasiquoter will generate the
initial sqlite database, and call this waterfall-picker program. With the mouse, the user
picks points on the model which are stored together with camera position and screen coordinates.
If the model changes, waterfall-picker will be able to replay the user's selection on the changed model,
which for small changes in a model viewed from the right point of view, will still give the correct points.

A [video](picker.webm) shows current capabilities.

## Build

    cmake .
    make
    ./waterfall-picker

## TODO

- [ ] keybinding to cycle between stls?
- [ ] paning camera?
- [ ] preview picks ie. draw the sphere for IsKeyDown
- [ ] mouse binding to rotate the light?
- [ ] checkerboard.png needs `.texcoords`
- [ ] argument parsing to replay? to review? without opening a window?
