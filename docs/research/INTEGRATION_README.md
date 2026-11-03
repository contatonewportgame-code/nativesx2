# Integração R639/R652 no `newportgame2`

Esta árvore é uma cópia de trabalho; ela não altera automaticamente `/home/cristiano/newportgame2`.

## O que foi integrado

A integração portou o contrato de evidência dos Rounds 639/652 e adicionou cinco checkpoints read-only no recompiler/runtime atual. Também incluiu `timer_thread.cpp` no CMake do runtime, pois a fonte existia mas não era linkada.

Não foram copiados runtimes inteiros, `output.c`, `output_runtime.c`, `ps2_game` ou binários de outra linha. O patch `patch_func_29a6c8.py`, que injeta `SignalSema(3)`, não foi executado nem aplicado.

## Aplicar no projeto local

Faça backup antes:

```bash
cd /home/cristiano
cp -a newportgame2 newportgame2.before_r639_r652
cd newportgame2
```

A partir da raiz do projeto, aplique o arquivo `newport2_r639_r652_integration.patch` com:

```bash
patch -p1 < newport2_r639_r652_integration.patch
```

Se o patch for aplicado em uma árvore diferente, pare e compare os arquivos em vez de forçar a aplicação.

## Regenerar e compilar

Na árvore local, confirme o ELF e regenere os arquivos a partir dele:

```bash
export PATH=/usr/bin:/bin:$PATH
ls -lh tools/ps2recomp/build/elf_out/SCUS_973.99.elf
cmake -S tools/ps2recomp -B tools/ps2recomp/build_clean
cmake --build tools/ps2recomp/build_clean -j2
./tools/ps2recomp/build_clean/ps2recomp recomp-elf \
  tools/ps2recomp/build/elf_out/SCUS_973.99.elf \
  tools/ps2recomp/build/output.c
python3 tools/ps2recomp/runtime/patch_output.py \
  tools/ps2recomp/build/output.c \
  tools/ps2recomp/build/output_runtime.c
cmake -S tools/ps2recomp/runtime -B tools/ps2recomp/runtime/build_clean
cmake --build tools/ps2recomp/runtime/build_clean -j2
```

## Executar somente análise headless

O ELF não deve ser executado diretamente. Para um smoke test controlado do runtime:

```bash
PS2_TRACE_R639_R652=1 \
  timeout 20s tools/ps2recomp/runtime/build_clean/ps2_game \
  --headless --frames 1 \
  tools/ps2recomp/build/elf_out/SCUS_973.99.elf
```

O trace é desativado por padrão. Ele apenas lê registradores e campos de memória. Não deve haver chamada direta a `SignalSema(3)` nas alterações integradas.

## Critério de sucesso

Build válido não é o mesmo que boot funcional. O critério funcional continua sendo publicação guest real em `0x002A5B10`, seguida por `SignalSema(3)`, `WAIT_SEMA_WAKE` da Thread 2, `WAIT_SEMA_SUCCESS` e progresso de frames/GS.
