# Investigation Notes — Newportgame4 / PS2 Static Recompiler

> ⚠️ **AVISO DE AUDITORIA (2026-09-01):** Este arquivo contém logs cronológicos de rodadas de investigação (2026-08-26 a 2026-08-31). Conclusões factuais foram corrigidas em `AUDITORIA_CONCLUSOES_2026-09-01.md`. **O texto abaixo é preservado integralmente como registro histórico**; as correções principais:
> - IOPRP300.IMG **existe** (não "apenas ELF EE")
> - "Cadeia causal" → **"Cadeia causal estática"**
> - Tradutor MIPS-I: **testado isoladamente com instruções reais dos ELFs**
> - SIF/ThreadMan/syscall 0x212: **apenas sintéticos**
> - `CDVDMAN\x1c.elf` = `loadelf`/`loadfile` (lib `LoadModuleByEE`), **não cdvdman**
> - `CDVDFSV .elf` = `intrman` (lib `Timer_Manager`), **não cdvdman**
> - Export `cdvdman:sceCdSC` link ID 0x32: **NÃO COMPROVADO**

---

## Round: COP0.Status EIE/EDI (2026-08-26)

A documentação PS2Tek descreve o registrador `COP0.Status` (12) como: bit 0 `IE` (interrupt enable), bits 1/2 `EXL`/`ERL`, bit 16 `EIE` (master interrupt enable) e bit 17 `EDI` (se `EDI` não estiver setado, `EI/DI` só funciona em modo kernel). Fonte: [PS2Tek — EE COP0 Registers](https://psi-rockin.github.io/ps2tek/).

Aplicação à rodada: o runtime estava limpando `Status.EIE` (`0x10000`) na emissão de todo `sync`, por meio do chamado `SYNC HACK`. Isso explica por que `func_299230` retorna `0` em uma chamada posterior com `ra=0x294b28`, fazendo o `beq v0, zero` cair em `0x294b68` e impedindo o caminho guest que prepararia `StartThread`. A semântica exata de restaurar EIE ainda não foi alterada; é necessário observar o contrato do par de rotinas no ELF antes de remover ou limitar o workaround.

A mesma seção do PS2Tek também afirma que as interrupções do EE só são habilitadas quando `Status.IE && Status.EIE && !Status.EXL && !Status.ERL`. Portanto, `EIE` não é um contador transitório de módulo: limpá-lo globalmente no `sync` altera a habilitação mestre de interrupções. O comportamento observado — uma chamada inicial de `func_299230` consumir/limpar EIE e uma chamada posterior retornar zero — deve ser tratado como possível semântica incompleta de `di/ei`/estado de interrupções, não como autorização para chamar `StartThread` pelo host.

A página do TX79/QEMU foi aberta para cross-check, mas o conteúdo dinâmico não expôs o corpo de `tx79_translate.c` no texto recuperado. Nenhuma conclusão sobre `EI/DI` foi baseada nesse resultado incompleto; a prova usada nesta rodada permanece o PS2Tek e a observação direta do ELF/C gerado.

## Round: EI/DI também controlam EIE (2026-08-26)

A discussão técnica da issue [PCSX2 #2139](https://github.com/PCSX2/pcsx2/issues/2139) descreve o bit de habilitação mestre: `DI` o limpa e `EI` o seta; o comentário também relaciona o bit ao estado global de interrupções. Isso é consistente com o padrão do ELF: `0x299230` testa EIE e espera/limpa o estado, enquanto `0x299280` lê o EIE antigo e executa `EI` para reativá-lo.

Conclusão operacional: o emissor local de `ei` que só faz `regs->cop0[12] |= 1u` não modela EIE e pode deixar o guest permanentemente com `EIE=0` depois da primeira sequência `DI/SYNC`. A próxima correção candidata é fazer `ei` setar `IE|EIE` e `di` limpar `IE|EIE`, validando antes/depois no mesmo caminho. Isso não chama `StartThread` nem sinaliza semáforo; é uma correção da semântica de COP0 sustentada por observação do ELF e pelas referências técnicas.

## Rodada scheduler/DI-EI — 2026-08-26

A semântica do recompiler para `DI/EI` foi corrigida para controlar `COP0.Status` nos bits `IE|EIE` (`0x10001`), em vez de alterar apenas `IE`; o `SYNC HACK` global que limpava EIE foi removido. O efeito observável é causalmente importante: o caminho guest passa pelo checker de interrupções, chama `StartThread (0x22)` com tid 2 e entrada `0x002947c8`.

Foi adicionado um driver cooperativo no ciclo de vida do runtime para retomar somente threads que o próprio guest colocou na fila via `StartThread`. O emissor agora marca `pc+4` após cada syscall e retorna da função C gerada quando o runtime marca a thread como `THS_WAIT`; assim `WaitSema` não gira dentro da pilha host. Em modo causal, o resultado é: `StartThread` real, `WaitSema(3)` real com count 0, `THS_WAIT`, retorno ao scheduler com `pc=0x00293c68`, fila vazia, `SignalSema` ausente, `SifComplete` ausente, `frames=0` e `gs_writes=0`.

Em bootstrap, o workaround legado de `StartThread` ainda injeta count 100 na semáforo 3. A Thread 2 então consome esses tokens e volta a bloquear; isso gera muitos `WaitSema-success`, mas não constitui progresso funcional. O mesmo run não deve ser usado para provar a cadeia `publicação guest -> SignalSema(3) -> wake -> frames`.

Foi adicionado um trace bounded `SIFCOMP`, keyed por `(slot_addr, sequence, obj_addr)`, que registra campos antes/depois e o resultado de dispatch somente quando `complete_sif_slot()` aceita o descritor. No bootstrap com o caminho corrigido, nenhum descritor foi aceito (`SIFCOMP=0`); portanto não há base para aplicar cursor/one-shot especulativo. O ciclo SIF antigo de tid 1 não reapareceu após a correção de `DI/EI`; a ausência atual é consistente com a falta de resposta/interrupto IOP real.

## Round: scheduler em fronteira segura e ELF válido (2026-08-26)

A instrumentação revelou que vários logs anteriores executavam com um caminho inexistente (`tools/ps2recomp/build/elf_out/SCUS_973.99.elf`). Neles, `[HOST] Cannot open ELF` tornava inválidos os alvos nulos observados em callbacks. O run correto usa `artifacts/input/SCUS_973.99.elf`, copia o PT_LOAD para a RAM guest e carrega a tabela `.rodata` de callbacks em `0x2B2DC0`.

Também foi comprovado que `ps2_scheduler_yield(regs)` dentro de `WaitSema`, `SleepThread`, `fire_intc_handlers` e `FlushCache` podia trocar o único `PS2Regs` no meio da execução de uma função guest. A correção no laboratório remove as trocas internas, preserva tid1 na fila enquanto está `THS_RUN`, e insere o quantum somente após `ps2_dispatch` no driver gerado, a cada 256 dispatches ou quando a thread está bloqueada. No run causal válido, o trace mostrou tid1 com snapshot em `pc=0x002894d4`, switch para tid2, bloqueio real em `WaitSema(3)` com count 0 e retorno para tid1 no mesmo PC. Não foi introduzido `$fp/$s8`, nem o paliativo de RA em `sp+64`.

Após a correção, o Timer2 é registrado e chamado pelo host, mas observa `0x002A5B10=0` em todas as entradas; o valor inicial também é zero no ELF, com `0x2A5B14=0xffffffff`. O único escritor gerado dessa variável continua sendo `func_299ce4`, que não é alcançada. O causal não apresenta publicação guest, `SignalSema(3)`, wake/success da Thread 2, SIFCOMP, frames ou GS. O bootstrap do mesmo build apenas consome os 100 tokens artificiais guardados no `StartThread`; isso não é evidência funcional.

Conclusão: o scheduler deixou de perder o contexto principal, mas o critério causal continua falhando por ausência do produtor/resposta IOP guest. O próximo trabalho deve localizar uma fonte IOP real ou executar um módulo IOP real, se disponibilizado; não aplicar completion host-side, cursor write, head fabricado ou sinalização artificial.


## Round: LQ/SQ, D0/VIF1, campo GS e fronteira SIF (2026-08-26)

O emissor MIPS tinha um bug real na metade alta de `LQ/SQ`: a representação do runtime usa `r[rt]` e `r_hi[rt]`, mas o código usava `r[rt+1]` como metade alta, clobberando registradores vizinhos em padrões como `lq $a1,16($a1)`. O emissor foi corrigido para usar `r_hi[rt]`, com snapshot/restauração coerente, e o recompiler foi recompilado seguido de regeneração integral do C a partir de `artifacts/input/SCUS_973.99.elf`.

A validação observável do fix ocorreu no FIFO VIF1 `0x10005000`: o segundo qword deixou de ser `lo=0x18, hi=0x40` e passou a `lo=0x0300000006000000, hi=0x0400000002000000`, correspondendo aos words guest `0x06000000, 0x03000000, 0x02000000, 0x04000000`. O primeiro qword observado foi `0x2000000001000404 / 0x0500000000000000`. Isso valida a semântica de transporte e o payload desse trecho, não uma execução completa de VIF/VU ou renderização.

O modelo experimental `PS2_MODEL_VIF0=1` foi restringido ao chain D0 real: `QWC=0`, `TADR=0x002B0F00`, `CHCR=0x145`, tag CNT `0x10000039` com QWC 57 em `0x002B0F00` e tag END `0x70000004` com QWC 4 em `0x002B12A0`. O parser de stream é read-only e auxiliar; dados de microprograma/VU não devem ser tratados como comandos VIF por classificação simplista. O baseline causal puro continua sem esse modelo e bloqueia no caminho BC0F.

A modelagem GS foi limitada ao que a referência PS2SDK permite afirmar: GS_CSR bit 13 é ODD/EVEN field. O reset guest em `GS_CSR=0x200` limpa o estado; cada IRQ2/VBlank alterna somente bit 13. Sem inventar FINISH/SIGNAL, writes GS ou frames, o ramo experimental atravessou `0x2940f0`, entrou no handler guest VBlank `0x00182f28` e chegou a `0x00296718`/`0x00294034`. Esse avanço é semântico e mensurável, porém experimental.

O novo bloqueio do ramo experimental está isolado em `296710/296718`: `SifGetReg(4)` via syscall `0x7A`, seguido de `& 0x20000`, retorna ao loop enquanto o bit está ausente. O trace chegou a aproximadamente 46 milhões de chamadas em 4 segundos. PS2Tek define `0x20000` como "SIFCMD initialized", uma resposta produzida pelo IOP durante SIFCMD Init. O inventário do laboratório não encontrou ELF, IRX, BIOS ou imagem IOP.

---

[CONTEÚDO HISTÓRICO PRESERVADO — RODADAS 2026-08-26 A 2026-08-31 CONTINUAM ABAIXO SEM ALTERAÇÕES]

---

## Round: auditoria estática de dependências IOP no ELF (2026-08-26) [L56-64]
## Round: resolução dos índices SIF e integridade causal dos reads físicos (2026-08-26) [L64-76]
## Round: auditoria física D6/SIF1 antes do polling SMFLAG (2026-08-26) [L76-84]
## Round: auditoria do stub `func_28f140` (2026-08-26) [L84-90]
## Round: despacho semântico de handlers registrados por `SetSyscall` (2026-08-26) [L90-103]
## Validação final do round `SetSyscall`/handlers EE (2026-08-26) [L103-112]
## Round: restauração dos corpos guest `func_29a970` e `func_28f140` (2026-08-26) [L112-121]
## Round: correção ABI `SifSetDma` versus `SifSetDChain` (2026-08-26) [L121-139]
 ### Retificação da contagem de DMA no handshake [L132-139]
## Round: logger SIF/D5 e validação oficial (2026-08-26) [L139-144]
## Round: ABI nominal, callgraph SIF e Timer3 (2026-08-26) [L144-150]
## Round: ponte IOP artifact-backed e validação causal final — 2026-08-26 [L150-155]
## Round: IRX ck01 embutido e reavaliação de func_27a810 [L155-162]
## Round — `17bc80` alcançada antes do gate SIF (2026-08-27) [L162-202]
 ### Round 17aa78 — correção de endereço e proveniência guest (2026-08-26/27) [L166-172]
 ### Round contrato IOP/SIF e backup final (2026-08-26/27) [L172-178]
 ### Round DMAC/SIF — 2026-08-27 [L178-184]
 ### Round SIFCMD — 2026-08-27 [L184-190]
 ### Round IOP readiness — 2026-08-27 [L190-196]
 ### Round protocol gate — 2026-08-27 [L196-202]
## 2026-08-27 — tabela guest de handlers SIFCMD e producers de RPC [L202-222]
 ### Round RPC handlers ABI — 2026-08-27 [L214-222]
## Round sema3/RPC/publicação — 29c4e4 [L222-233]
## Round RPC client/sema_id — 29c4e4 [L233-242]
## Round estado global RPC/worker e sema 3 — 29c4e4 [L242-251]
## Round mapa de clientes RPC — 29c4e4 [L251-260]
## Round proveniência de `CreateSema` guest — 29c4e4 [L260-268]
## Round dataflow de consumidores de semáforo — 29c4e4 [L268-276]
## Round proveniência SIFCMD → RPC_CALL → iSignalSema(3) — 29c4e4 [L276-284]
## Round correlação iSifSetDChain/D5 e descriptors SIF — 29c4e4 [L284-292]
## Round origem de `cmd_data+0`/`pktbuf` D5 — 29c4e4 [L292-300]
## Round armamento guest D5/DMAC SIF0 — 29c4e4 [L300-306]
## Round matriz EE/IOP e descriptors SIF/DMAC — 29c4e4 [L306-312]
## Round inventário de artefatos IOP/BIOS/IOPRP/IRX — 29c4e4 [L312-318]
## Round compatibilidade causal versus linha Zed — 29c4e4 [L318-324]
## Round stack/RA/continuations MIPS — 29c4e4 [L324-330]
## Round `jalr`/delay slot e continuation D5 — 29c4e4 [L330-336]
## Round comparação `output.c`/`output_runtime.c`, dispatcher e targets críticos — 29c4e4 [L336-341]
## Round closure do dispatcher, overrides e alvos indiretos — 29c4e4 [L341-346]
## Round superfície de stubs intra-TU e guards causais — 29c4e4 [L346-351]
## Round reavaliação do provider IOP artifact-backed — 29c4e4 [L351-356]
## Round IRX IOP embutido e cadeia guest de staging — 29c4e4 [L356-363]
## Round etapa loader pós-staging e gates de evento — 29c4e4 [L363-370]
## **Round — Gate COP0/DMA da etapa loader — 29c4e4** [L370-375]
## **Round — SifDmaStat e ID pendente: cadeia guest de completion — 29c4e4** [L375-380]
## **Round — SifGetReg/SifSetReg: handshake guest e ACK externo — 29c4e4** [L380-385]
## **Round — SifRpcCall e resposta: fronteira RPC causal — 29c4e4** [L385-390]
## **Round — Semáforo 3: sinal guest legítimo versus workaround host — 29c4e4** [L390-396]
## **Round — Primeiro producer externo D5: do armamento guest à chegada IOP/SIF0 — 29c4e4** [L396-404]
## **Round — Primeiro producer `CMDINIT`: fronteira IOP→EE antes do D5 — 29c4e4** [L404-412]
## **Round — Primeiro descriptor EE→IOP: `CMDINIT` não é resposta a submit local anterior — 29c4e4** [L412-420]
## **Round — Staging `ck01`: descriptor EE→IOP real e gap lawful de loader — 29c4e4** [L420-430]
## **Round — Loader IOP e `CMDINIT`: descriptor staged não é provider SIF — 29c4e4** [L430-440]
## **Round — Contrato mínimo `sifman`/`sifcmd`: `CMDINIT` exige peer IOP real — 29c4e4** [L440-448]
## **Round — Proveniência `SIF_CMD_INIT_CMD`: protocolo conhecido, packet guest não observado — 29c4e4** [L448-456]
## **Round — Surface guest SIF: ausência formal de callsite `SIF_CMD_INIT_CMD` — 29c4e4** [L456-464]
## **Round — Writers SIF: proveniência consolidada de `SMFLAG`, `SUBADDR` e `RPCINIT` — 29c4e4** [L464-472]
## **Round — Revalidação dos writers SIF: nenhum provider novo e causalidade preservada — 29c4e4** [L472-480]
## **Round — Writers guest SIF: classificação guest-owned versus producer IOP — 29c4e4** [L480-490]
## **Round — Alcançabilidade dos writers guest SIF: staticamente presentes, causalmente não alcançados — 29c4e4** [L490-500]
## **Round — Dependência causal do semáforo 3: RPC real antes de `iSignalSema(-0x43)` — 29c4e4** [L500-508]
## **Round — Objeto/slot de resposta RPC: projeção guest conhecida, proveniência IOP ainda ausente — 29c4e4** [L508-516]
## **Round — Descriptor D5 e slot RPC: estruturas distintas, completion causalmente ausente — 29c4e4** [L516-524]
## **Novo Round — Callback D5→RPC: `a0=$sp`, projeção `+52`, origem ainda não observada — 29c4e4** [L524-532]
## **Novo Round — Registro de handlers e `a1` RPC: contexto registrado não é o slot de resposta — 29c4e4** [L532-540]
## **Próximo Round — Contexto guest `0x3292C0` versus frame RPC `a0+52` — 29c4e4** [L540-548]
## **Próximo Round — Proveniência lawful `pktbuf→frame+52→RPC` — 29c4e4** [L548-556]
## **Novo Round — Writers de `pktbuf`: initializer sem preenchimento local — 29c4e4** [L556-562]
## **Novo Round — Aliases de `pktbuf`: nenhum producer guest conhecido — 29c4e4** [L562-568]
## **Novo Round — Endpoint D5/SIF0, `SMFLAG=4` e tabela de recepção — 29c4e4** [L568-576]
## **Novo Round — Superfície `TADR/CHCR` e chain descriptors D5 — 29c4e4** [L576-664]
 ### Round bases de chain D5 — descritores guest e fronteira de cobertura — 2026-08-27 [L585-593]
 ### Round aliases SIF e proveniência ELF — 2026-08-27 [L593-601]
 ### Round família alternativa 0x2C78xx — 2026-08-27 [L601-609]
 ### Round superfície SIF e argumentos DChain — 2026-08-27 [L609-619]
 ### Round continuations SIF tardias — 2026-08-27 [L619-629]
 ### Round rastreamento `SUBADDR/RPCINIT/SMFLAG` — 2026-08-27 [L629-639]
 ### Round callers upstream, boot/dispatcher e `sregs` — 2026-08-27 [L639-647]
 ### Round instalação de handlers SIF/RPC e fronteira `cmd_data`/`pktbuf` — 2026-08-27 [L647-655]
 ### Round dispatch indireto `0x00296B68` e handler table — 2026-08-27 [L655-664]
## **Novo Round — Slots vivos da handler table: `r6/r5` antes do `jalr`** [L664-678]
## **Novo Round — Reverse-map de `0x00296B68` e inventário dos slots vivos** [L678-692]
## **Novo Round — Proveniência de índice/stride e aliases upstream da handler table** [L692-708]
## **Novo Round — Callers upstream dos writers e cruzamento handler↔SIF** [L708-720]
## **Novo Round — Dataflow de argumentos upstream até os slots da handler table** [L720-732]
## **Novo Round — Writers e consumidores locais dos contextos registrados** [L732-744]
## **Novo Round — Proveniência de aliases e resolução de accesses dos contextos** [L744-756]
## **Novo Round — Bases relativas nos corpos C nomeados: stack versus guest** [L756-768]
## **Novo Round — Dataflow de `r4/r5/r6/r17/r18` nos callers e continuations** [L768-780]
## **Novo Round — Aliases opacos de `r16/r17/r18` nas closures locais** [L780-792]
## **Novo Round — Separação stack/frame versus aliases guest `r16/r17/r18`** [L792-804]
## **Novo Round — Continuations: cópias de `r28/r29`, retornos e destinos** [L804-814]
## **Novo Round — Restauração de `r31`, retornos e fronteiras de contexto** [L814-824]
## **Novo Round — Proveniência da continuidade `0x00296B60→0x00296B68→handler`** [L824-832]
## **Novo Round — Reverse-map de `r6` contra os cinco handler targets** [L832-842]
## **Novo Round — Proveniência das registrations guest-side e separação do payload D5** [L842-850]
## **Novo Round — Proveniência das bases `r2/r16/r17` na formação de `entry`** [L850-858]
## **Novo Round — Chaves e stride dos sete callers de registration** [L858-866]
## **Novo Round — Formas constantes de `r4/r5` nos sete callers de registration** [L866-874]
## **Novo Round — Continuations pós-registration e fronteiras de retorno** [L874-882]
## **Novo Round — Proveniência de `r31` nas continuations de registration** [L882-890]
## **Novo Round — Censo global e restores de `r31` no intervalo causal** [L890-898]
## **Novo Round — Encadeamento `r29` stack→`r31`→`pc` no intervalo causal** [L898-904]
## **Novo Round — Bases `r28/r29` no owner do dispatcher `0x00296B60/0x00296B68`** [L904-910]
## **Novo Round — Transporte local de `r17` entre predecessor, dispatcher e registration** [L910-916]
## **Novo Round — Dataflow local de `r17` até `jalr` em `0x00296B68`** [L916-922]
## **Novo Round — Caminho `r6→r31` e continuations pós-`jalr`** [L922-928]
## **Novo Round — Continuidade de `r31` além do epílogo e retorno ao caller** [L928-934]
## **Novo Round — Coerência de `r29` e offsets de frame** [L934-940]
## **Novo Round — Coerência de `r28` entre dispatcher, predecessor e registration** [L940-946]
## **Novo Round — Relação local de `r28` consumido e `r16` derivado** [L946-952]
## **Novo Round — Continuations de `r28` após aliases e relação estrutural com `r16`** [L952-958]
## **Novo Round — Separação de `r17` como base de memória versus `r28/r16` como aliases** [L958-964]
## **Novo Round — Censo global de `r16` fora dos owners já fechados** [L964-970]
## **Novo Round — Ocorrências de `r16` em regiões de call/return** [L970-976]
## **Novo Round — Ocorrências de `r16` próximas a restores de frame e epílogos** [L976-982]
## **Novo Round — Distribuição global de `r16` por função e distância às regiões causais** [L982-988]
## **Novo Round — Concentração de `r16` nos primeiros spans de função** [L988-994]
## **Novo Round — Segmentação numérica de `r16` por bucket de endereço** [L994-1000]
## **Novo Round — Distribuição de `r16` em faixas finas de endereço** [L1000-1006]
## **Novo Round — Sobreposição numérica da janela causal com `r16`** [L1006-1012]
## **Novo Round — Densidade source-level de `r16` na janela causal** [L1012-1018]
## **Novo Round — Fração source-level de `r16` na janela causal no censo global** [L1018-1024]
## **Novo Round — Origem textual de `r6` nas duas reads indiretas** [L1024-1030]
## **Novo Round — Continuidade textual de `r6` até o `jalr` comum** [L1030-1036]
## **Novo Round — Caminhos zero/não-zero após as loads de `r6`** [L1036-1042]
## **Novo Round — Coerência dos epílogos e retornos via `r31`** [L1042-1048]
## **Novo Round — Mapeamento PC→restore no epílogo compartilhado** [L1048-1054]
## **Novo Round — Inventário externo de retornos via `r31`** [L1054-1060]
## **Novo Round — Distribuição source-level dos offsets de `r31`** [L1060-1066]
## **Novo Round — Alinhamento source-level comentário-PC→`r31` externo** [L1066-1072]
## **Novo Round — Forma source-level dos owners externos de `PC=r31`** [L1072-1078]
## **Novo Round — Padrões source-level de retorno externo via `r31`** [L1078-1084]
## **Novo Round — Distribuição lexical de restores adicionais em owners externos** [L1084-1090]
## **Novo Round — Ordem lexical dos restores adicionais antes de `PC=r31`** [L1090-1096]
## **Novo Round — Posição lexical do ajuste de `r29` antes de `PC=r31`** [L1096-1102]
## **Novo Round — Delta lexical entre o último ajuste de `r29` e `PC=r31`** [L1102-1110]
## **Novo Round — Tail lexical após o restore de `r31` até `return`** [L1110-1118]
## **Novo Round — Fechamento lexical após `PC=r31` até `return;`** [L1118-1126]
## **Novo Round — Returns lexicais múltiplos e ordem relativa a `PC=r31`** [L1126-1132]
## **Novo Round — Posição lexical do primeiro e último `return;` relativo a `PC=r31`** [L1132-1138]
## **Round — Proveniência lexical de publicação `0x002A5B10` e semáforos — 29c4e4** [L1138-1149]
## **Round — Predecessores lexicais de `WaitSema` e `iSignalSema` — 29c4e4** [L1149-1160]
## **Round — Ciclo lexical do handle semafórico em `0x00326F40` — 29c4e4** [L1160-1171]
## **Round — Callers e retorno source-level de `CreateSema(64)` — 29c4e4** [L1171-1182]
## **Round — Formas lexicais de retorno e stores pós-`CreateSema(64)` — 29c4e4** [L1182-1193]
## **Round — Matriz source-level de registro e dispatch dos PCs causais — 29c4e4** [L1193-1202]
## **Round — Pontes source-level entry→wrapper→retorno — 29c4e4** [L1202-1211]
## **Round — Contrato runtime `ps2_syscall`→scheduler — 29c4e4** [L1211-1222]
## **Round — Proveniência dos callsites de `ps2_scheduler_yield` — 29c4e4** [L1222-1231]
## **Round — Restauração de contexto `regs_snapshot`/`pc_snapshot` — 29c4e4** [L1231-1239]
## **Round — Observação official da fronteira scheduler/contexto — 29c4e4** [L1239-1245]
## **Round — Proveniência de `SignalSema(3)` e lacuna do produtor lawful — 29c4e4** [L1245-1251]
## **Round — Topologia SIF0 e ausência do peer IOP lawful — 29c4e4** [L1251-1257]
## **Round — Census exaustivo de archives e lacuna IOP lawful — 29c4e4** [L1257-1263]
## **Round — Contrato externo IOP/SIF/RPC lawful — 29c4e4** [L1263-1269]
## **Round — Intake passivo de artefatos IOP/IOPRP — 29c4e4** [L1269-1275]
## **Round — Superfície interna de IRQ host-side versus provider IOP — 29c4e4** [L1275-1281]
## **Round — Handler guest Timer2 `0x00299D08` e boundary host/IOP — 29c4e4** [L1281-1286]
## **Round — Ordem temporal `WaitSema(3)` versus callbacks Timer2 — 29c4e4** [L1286-1291]
## **Round — Correlação dos dois Timer2 pré-bloqueio com a Thread 2 — 29c4e4** [L1291-1296]
## **Round de quiescência — Intake passivo sem provider IOP novo — 29c4e4** [L1296-1300]

---

*Fim do registro histórico. Ver `AUDITORIA_CONCLUSOES_2026-09-01.md` para o estado corrigido atual.*