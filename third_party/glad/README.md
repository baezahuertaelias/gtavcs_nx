# GLAD provenance

Generated locally with `glad2==2.0.8`, reproducible bundled specifications,
OpenGL 3.3 core, zero optional extensions, no OS dynamic loader. SDL supplies
function resolution through `SDL_GL_GetProcAddress`.

Regeneration from the repository root (host Python, not Switch Python):

```bash
python3 -m venv build/host-tools
build/host-tools/bin/python -m pip install glad2==2.0.8
build/host-tools/bin/python -m glad --api gl:core=3.3 --extensions '' \
  --out-path third_party/glad --reproducible c
```

On Windows use `build/host-tools/Scripts/python.exe` instead. Preserve the
license/SPDX notices in `src/gl.c`, `include/glad/gl.h`, and
`include/KHR/khrplatform.h`. The generated loader is a CMake static target named
`glad`, not a devkitPro package. [Generator source](https://github.com/Dav1dde/glad/tree/v2.0.8).
