# Matriz de compatibilidade R639/R652 → `newportgame2`

## Estado da integração

Esta é uma portabilidade controlada. O código atual de `newportgame2` permanece a fonte de verdade. Os Rounds 639/652 são usados como evidência e mapa causal, não como fonte para substituir arquivos inteiros.

## Pode ser aproveitado diretamente como referência

| Elemento | Tratamento | Motivo |
|---|---|---|
| Critério `0x002A5B10 → SignalSema(3) → WAIT_SEMA_WAKE/SUCCESS → frames/GS` | PORTAR como critério de validação | É um contrato de observação, não uma implementação dependente de ABI. |
| Conclusão R639 sobre `func_27A648` e `a0=1` | USAR SOMENTE COMO REFERÊNCIA | Depende da mesma imagem ELF/endereço e deve ser revalidada no `newportgame2`. |
| Conclusão R652 sobre `26CA18 → 26BF28 → 26BB98` | USAR SOMENTE COMO REFERÊNCIA | Deve ser confirmada no ELF e C gerado usados pelo `newportgame2`. |
| Regras read-only das probes | ADAPTAR | O runtime atual tem APIs e estruturas diferentes. |
| Separação entre eventos guest e host/probes | PORTAR como regra de análise | Reduz falsos positivos e não altera semântica. |
| Scripts de análise estática | ADAPTAR | Alguns assumem nomes, caminhos e layouts da outra linha. |

## Não pode ser copiado em bloco

| Elemento | Tratamento | Motivo |
|---|---|---|
| `recompiler.cpp` inteiro | REJEITAR cópia em bloco | Hash, tamanho e implementação divergem. |
| `bios_stub.cpp`/`host_main.cpp` inteiros | REJEITAR cópia em bloco | ABI, scheduler e instrumentação diferem. |
| `ps2_runtime_data.c` inteiro | REJEITAR cópia em bloco | O R652 tem aproximadamente 10.512 linhas contra aproximadamente 1.040 no `newportgame2`. |
| `output.c`, `output_runtime.c`, `ps2_game`, objetos e caches | REJEITAR cópia | Devem ser regenerados no `newportgame2` a partir do ELF local. |
| `patch_func_29a6c8.py` | REJEITAR | Injeta `SignalSema(3)` diretamente e viola o critério read-only. |
| Overrides que forçam branches, PC, retornos ou filas | REJEITAR | Produzem avanço artificial. |

## Fases aprovadas

1. Corrigir/regenerar o pipeline atual do `newportgame2`.
2. Confirmar que o C gerado compila sem fallthrough fora de função.
3. Portar apenas uma probe observacional por vez, usando as APIs do runtime atual.
4. Executar o headless e registrar os critérios exatos, sem injeção de semáforo.
5. Comparar com R639/R652 somente no nível causal demonstrado.
