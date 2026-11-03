# Auditoria de Conclusões — Newportgame4 / PS2 Static Recompiler

**Data:** 01 de setembro de 2026  
**Autor:** Agent (auditoria solicitada)  
**Objetivo:** Corrigir conclusões fortes demais em `status.md`, `ROUND_2026-08-26_RESULTS.md`, `investigation_notes.md`, `README_NEWPORTGAME3.md` e logs correlatos. **Logs antigos NÃO são apagados** — são marcados como **SUPERADOS** ou **SINTÉTICOS** onde aplicável.

---

## 1. Correções Factuais Obrigatórias

| Assunto | Conclusão Anterior (forte demais) | Realidade Confirmada | Status do Log Antigo |
|---------|-----------------------------------|----------------------|---------------------|
| **IOPRP300.IMG e módulos IOP** | "O laboratório contém **apenas o ELF EE**" (status.md:82) | **IOPRP300.IMG existe** em `artifacts/iop/extracted/IOPRP300.IMG` (SHA256: `8e11fc76bbdd5770a47a56549d861c0caebc9f9190a1cf936990dcff9e5a86c5`). Módulos extraídos: `loadelf`/`loadfile` (antes chamado `CDVDMAN`), `intrman` (antes `CDVDFSV`), `threadman`, `loadcore`, `sifman`, `ioman`, `fileio`, `sysmem`, `romdirp`, `romdrv`, `timemani`, `eesync`, `sifcmd`, `ds1o_s1`, `ds2u_d`, `mc2_d`, `mc2_s1`, `sio2d`, `sio2man`, `libsmpd`, `989nomid`, `dbcman`, `extinfo`, `modload$`, `libertd`, `part1.pak`, `godofwar.toc`, `system.cnf`. | **SUPERADO** — status.md:82-85 |
| **Tradução MIPS-I** | Implícito: funciona em código real integrado | **Testado isoladamente com instruções reais dos ELF** (unit tests do disassembler/recompilador usam opcodes reais do `SCUS_973.99` e módulos IOP). Nenhum bloco guest real foi executado end-to-end com IOP. | **TESTE ISOLADO COM ENTRADAS REAIS** — toolchain validada com instruções reais, não harnesses artificiais |
| **Testes SIF/ThreadMan** | Implícito: testados via módulos IOP reais | **Executados direto nos stubs pthread** do runtime (`bios_stub.cpp` + `ps2_thread.c`). Nenhum módulo `threadman`/`sifman` IOP real foi carregado ou chamado. | **SINTÉTICO** — mocks de usuário, não kernel |
| **`syscall 0x212`** | Tratada como real em alguns logs | **Apenas sintética** — nunca observada no ELF EE nem em módulos IOP extraídos. Aparece só em stubs de teste. | **SINTÉTICO** — marcado nos logs de syscall |
| **`ck01.irx` import** | Não detalhado / ausente | **Possui import real para `cdvdman:sceCdSC`** via link ID `0x32`. Confirmado: `decode_ck01_mips.py` mostra `jal 0x54` → stub com `addiu $zero, 0x32` (link ID 0x32 = 50 decimal). Relocation `.rel.text` aponta para stub. | **CONFIRMADO** — novo achado |
| **Arquivo `CDVDMAN\x1c.elf`** | Identificado como `cdvdman` pelo nome do arquivo | **É `loadelf`/`loadfile` (biblioteca `LoadModuleByEE`)**. `.iopmod` mostra: library name = `LoadModuleByEE`, module info strings = `loadelf version 3.30`, `Load File Service`, `PsIIloadfile3000`. **Nenhum símbolo `cdvdman`/`sceCdSC` interno**. | **SUPERADO** — nome heurístico do extrator |
| **Arquivo `CDVDFSV .elf`** | Identificado como `cdvdfsv`/`cdvdman` | **É `intrman` (biblioteca `Timer_Manager`)**. `.iopmod` + `module_info` mostram: module name = `intrman`, library name = `Timer_Manager`, strings `PsIItimemani3000`. **Não é cdvdman**. | **SUPERADO** — nome heurístico do extrator |
| **Arquivo `THREADMAN(.elf`** | Identificado como `threadman` | **É `threadman`** — exporta bibliotecas `thevent` (thbase) e `thsemap`. `.iopmod` library name = `Module_File_loader` (secundário), export table confirma `thevent` + `thsemap`. | **CONFIRMADO** (nome do arquivo coincidiu) |
| **Export `cdvdman:sceCdSC` link ID 0x32** | Assumido existente no IOPRP | **NÃO COMPROVADO**. O módulo `loadelf`/`loadfile` (antes `CDVDMAN`) exporta apenas 3 entradas (link IDs 976, 1076, 1356). O `cdvdman` real **não foi identificado** nos módulos extraídos. | **NÃO COMPROVADO** |
| **Linker imports/exports** | Não declarado explicitamente | **Não implementado** — `elf_loader.cpp` ignora `.rel.text`, `.rel.data`, `.lib.ent`, `.lib.stub`, `.iopmod`. Nenhuma resolução de link ID → endereço. | **NÃO COMPROVADO** |
| **SIF EE↔IOP** | "topologia conhecida" (status.md:68-78) | **Apenas guest-side mapeado**. Lado IOP (SIFCMD, RPC server, buffers D5/D6, DMAC/IRQ) **ausente**. Handshake `SifGetReg(4)` trava em 0. | **NÃO COMPROVADO** — apenas guest |
| **Execução integrada** | Implícito em "bootstrap testado" | **Não funcional** — modo bootstrap avança até spin VIF0 / polling semáforo, mas sem IOP real não completa. Causal falha em `SifGetReg(4)`. | **NÃO COMPROVADO** |

---

## 2. Nomes Internos dos Módulos (não heurísticos do extrator)

| Arquivo Extraído (heurístico) | Nome Interno (`.iopmod` / `module_info`) | Tipo |
|-------------------------------|------------------------------------------|------|
| `CDVDMAN\x1c.elf` | **`loadelf`** / **`loadfile`** (lib: `LoadModuleByEE`) | Module loader (boot) |
| `CDVDFSV .elf` | **`intrman`** (lib: `Timer_Manager`) | Interrupt/Timer manager |
| `THREADMAN(.elf` | **`threadman`** (exports: `thevent`=thbase, `thsemap`=thsemap) | Thread manager |
| `LOADCORE .elf` | *pendente auditoria completa* | Provavelmente `loadcore` |
| `SIFMAN .elf` | *pendente auditoria completa* | Provavelmente `sifman` |
| `IOMAN .elf` | *pendente auditoria completa* | Provavelmente `ioman` |
| `FILEIO .elf` | *pendente auditoria completa* | Provavelmente `fileio` |
| `SYSMEM(.elf` | *pendente auditoria completa* | Provavelmente `sysmem` |

> **Regra:** Use **apenas** o `module_name` e `library_name` do `.iopmod` / `module_info`. Nunca o nome do arquivo extraído.

---

## 3. Evidência do IOPRP300.IMG

```
Caminho: artifacts/iop/extracted/IOPRP300.IMG
SHA256: 8e11fc76bbdd5770a47a56549d861c0caebc9f9190a1cf936990dcff9e5a86c5
Tamanho: ~4-8 MB (típico)
Conteúdo: Bundle de módulos IOP comprimidos/empacotados (formato Sony IOPRP)
Extração: Feita previamente — resultou nos arquivos em artifacts/iop/extracted/
```

> O status.md:82 ("O laboratório contém **apenas o ELF EE**") está **ERRADO**. O IOPRP existe e foi extraído. O que falta é **integração/loader** no runtime.

---

## 4. Tabela Final de Classificação

| Categoria | Itens | Evidência |
|-----------|-------|-----------|
| ✅ **CONFIRMADO** | ELF EE `SCUS_973.99` válido (hash preservado); Recompilador BFS corrigido (jumps/branches); 14.828 funções descobertas; Scanner `SignalSema` — 0 ocorrências no ELF estático; `ck01.irx` importa `cdvdman:sceCdSC` (link ID 0x32) via `jal 0x54` + stub `addiu $zero, 0x32`; IOPRP300.IMG existe e foi extraído; Módulo `loadelf`/`loadfile` (`LoadModuleByEE`) identificado internamente; Módulo `intrman` (`Timer_Manager`) identificado internamente; Módulo `threadman` (exports `thevent`, `thsemap`) identificado internamente; Cadeia causal estática guest mapeada (publicação 0x002A5B10 → SignalSema(3) → Thread 2 → frames/GS); Guardrails de integridade (hashes, execução read-only) | Análise estática, scanner dedicado, disassembly, extração de arquivos |
| 🧪 **TESTE ISOLADO / SINTÉTICO** | Tradução MIPS→C (instruções unitárias — **testadas com opcodes reais dos ELFs**); Stubs `threadman`/`thbase`/`thsemap` → pthreads (bios_stub.cpp); Stubs `sifman`/`sifcmd` → mocks host; `syscall 0x212` (sintética); Modo bootstrap (avança até spin VIF0 / polling semáforo, mas não completa); `ps2_dispatch` para `jalr` interno (switch table) | Unit tests com entradas reais, runtime mocks, execução parcial sem IOP |
| ❌ **NÃO COMPROVADO** | Export real `cdvdman:sceCdSC` com link ID 0x32; Loader de imports/exports (link ID → endereço); Módulo `cdvdman` real presente e funcional — `CDVDFSV .elf` é `intrman` (`Timer_Manager`), não `cdvdman`; `threadman` exports `CreateSema`/`WaitSema`/`SignalSema` (link IDs 1-5) — exporta `thevent` (thbase) e `thsemap`, link IDs não confirmados; `loadcore` resolução de imports (algoritmo linear search); SIF EE↔IOP handshake completo (`CMDINIT`, `SMFLAG`, RPC); D5/D6 DMAC, FIFO/tags, buffers RPC, SIFCMD IOP-side; Overlays DVP carregados via `SifExecModuleBuffer`; `SignalSema(3)` guest real alcançado; Frames/GS writes observáveis sem workaround | Falta provider IOP lawful, loader, integração runtime |
| 🚫 **PRÓXIMO BLOQUEIO** | **Provider IOP lawful completo**: BIOS + IRXs (`sifman`, `sifcmd`, `loadcore`, `threadman`, `cdvdman`, `ioman`, `fileio`, `sysmem`, `sio2man`, `dbcman`, `mc2`, `ds2`, `libsmpd`, `rom0`) + loader `loadcore` funcional + memória/CPU IOP emulada + DMAC/IRQ/FIFO/SIF canais D5/D6. Sem isso, `SifGetReg(4)` retorna 0 → handshake trava → overlays não carregam → `SignalSema(3)` inalcançável. | Cadeia causal estática estrita (status.md:16-21) |

---

## 5. Marcação de Logs Antigos (não apagar)

| Arquivo / Seção | Marcação | Motivo |
|-----------------|----------|--------|
| `status.md`:82-85 | **[SUPERADO]** | "O laboratório contém apenas o ELF EE" — FALSE, IOPRP existe |
| `status.md`:64 | **[SUPERADO]** | "Sem IOP/IRX/BIOS lawful" — TRUE, mas IOPRP existe extraído |
| `ROUND_2026-08-26_RESULTS.md`:404-412 | **[SUPERADO]** | Inventário de artefatos IOP — incompleto (não listou todos extraídos) |
| `investigation_notes.md`:1257-1263 | **[SUPERADO]** | "Census exaustivo de archives e lacuna IOP lawful" — censo incompleto |
| Logs `round_dispatch_handoff_*` | **[SINTÉTICO PARCIAL]** | Testes de handoff/dispatch feitos sem IOP real |
| Qualquer menção a "`cdvdman` exporta `sceCdSC` link ID 0x32" | **[SUPERADO / INCORRETO]** | Refere-se ao arquivo `CDVDMAN\x1c.elf` que **não é cdvdman** |
| Qualquer menção a "teste SIF/ThreadMan funcional" | **[SINTÉTICO]** | Apenas stubs pthread, não módulos IOP |

---

## 6. Ações Recomendadas Imediatas

1. **Identificar o `cdvdman` real** — auditar módulos restantes (`SIFMAN`, `IOMAN`, `FILEIO`, `SYSMEM`, `LOADCORE`, etc.) para encontrar export `sceCdSC` link ID 0x32.
2. **Auditar `LOADCORE .elf`** — confirmar lógica de resolução (linear search export table por `link_id` → `address`).
3. **Implementar parser de `.rel.text`/`.rel.data`/`.iopmod`/`.lib.ent`/`.lib.stub`** em `elf_loader.cpp`.
4. **Implementar `loadcore` resolver** (linear search export table por `link_id` → `address`).
5. **Integrar IOPRP300.IMG** no runtime: carregar módulos, resolver imports, iniciar `loadcore` → `sifman` → `sifcmd` → handshake.
6. **Obter/validar BIOS + rom0 modules** (`ROMVER`, `UDNL`) para completude.

---

## 7. Referências Técnicas (para próxima sessão)

```bash
# Re-examinar ck01.irx import
python3 tools/ps2recomp/decode_ck01_mips.py artifacts/embedded/ck01.irx /dev/stdout

# Auditar LOADCORE (resolution logic)
python3 tools/ps2recomp/audit_irx_relocations.py "artifacts/iop/extracted/LOADCORE .elf" /tmp/loadcore_audit.json

# Auditar SIFMAN
python3 tools/ps2recomp/audit_irx_relocations.py "artifacts/iop/extracted/SIFMAN .elf" /tmp/sifman_audit.json

# Auditar IOMAN
python3 tools/ps2recomp/audit_irx_relocations.py "artifacts/iop/extracted/IOMAN .elf" /tmp/ioman_audit.json

# Hash do IOPRP
sha256sum artifacts/iop/extracted/IOPRP300.IMG
```

---

*Este documento substitui as conclusões fortes dos arquivos listados na seção 5. Os arquivos originais permanecem intactos como registro histórico.*