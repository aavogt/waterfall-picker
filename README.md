# waterfall-picker

This is a c raylib program for the following database:

![schema](schema.png}

[waterfall cad](https://github.com/joe-warren/opencascade-hs#readme) has no
feature to pick points on a model. A separate quasiquoter will generate the
initial sqlite database, and call waterfall-picker. With the mouse, the user
picks points on the model. If the model changes, waterfall-picker will be able
to replay the user's selection on the changed model.

## Build

    cmake .
    make
    ./waterfall-picker

## TODO

- [ ] mouse binding to rotate the light?
- [ ] checkerboard.png doesn't seem to be be attached though it is somehow necessary?
- [ ] make checkerboard.png a part of the executable
- [ ] ProcessInput() always says "No hit detected".
  - [ ] test DeletePick()
- [ ] argument parsing to replay? to review? without opening a window?
- [ ] DrawUI() y spacing off
