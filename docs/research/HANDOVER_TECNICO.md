# Relatório de Handover Técnico: Portabilidade de Jogo PS2 para PC (God of War)

Este documento descreve o estado atual do projeto de conversão estática do jogo de PlayStation 2 (executável `SCUS_973.99`) para PC (Linux/Windows), utilizando o fluxo de trabalho do **Ghidra** e do **PS2Recomp**.

## 1. Resumo do Progresso Realizado

Até o momento, as seguintes etapas críticas foram concluídas com sucesso:

1.  **Análise no Ghidra**: O executável original do PS2 (`SCUS_973.99`) foi analisado no Ghidra.
2.  **Exportação de Funções**: O script `ExportPS2Functions.java` foi executado no Ghidra, gerando um mapeamento completo das funções do jogo em formato TOML e CSV.
3.  **Recompilação Estática**: Utilizamos a ferramenta **PS2Recomp** para processar as exportações do Ghidra. Isso resultou na geração de **5.629 arquivos C++**, onde cada instrução MIPS original foi traduzida para uma operação C++ equivalente.
4.  **Estruturação do Projeto PC**: Criamos um ambiente de build baseado em **CMake** que integra o código recompilado com o `ps2xRuntime`.

## 2. Estrutura de Arquivos Atual

Os arquivos estão organizados no diretório `/home/ubuntu/god_pc_port/`:

*   **`include/`**: Contém os cabeçalhos das funções recompiladas (`ps2_recompiled_functions.h`) e as definições de stubs (`ps2_recompiled_stubs.h`).
*   **`src/recompiled/`**: Contém os milhares de arquivos `.cpp` gerados (um para cada função do jogo).
*   **`src/main.cpp`**: O ponto de entrada do programa no PC, responsável por inicializar o hardware virtual e registrar as funções.
*   **`CMakeLists.txt`**: Configuração de compilação que une o código do jogo, o runtime do PS2Recomp e a biblioteca gráfica **Raylib**.

## 3. Como o Sistema Funciona

O projeto utiliza uma abordagem de **Recompilação Estática**:
*   O **Runtime** emula o mapa de memória do PS2, os registradores da CPU (R5900) e as chamadas de sistema (Syscalls).
*   As funções do jogo original agora são funções C++ nativas que manipulam esse estado do hardware virtual.
*   O arquivo `register_functions.cpp` mapeia os endereços de memória originais do PS2 para as novas funções C++ no PC.

## 4. Próximos Passos (Roteiro para o Próximo Agente)

Para continuar o projeto, o próximo agente deve focar nas seguintes tarefas:

### Fase A: Compilação e Linkagem
1.  **Instalar Dependências**: Garantir que a biblioteca `raylib` esteja instalada no sistema (`libraylib-dev`).
2.  **Build do Projeto**: Executar o CMake para gerar o executável.
    ```bash
    mkdir build && cd build
    cmake ..
    make -j$(nproc)
    ```
3.  **Correção de Erros de Compilação**: Resolver possíveis conflitos de nomes ou tipos nos arquivos gerados (comum em projetos dessa escala).

### Fase B: Depuração e Implementação de Stubs
1.  **Execução Inicial**: Rodar o executável gerado apontando para o ELF original: `./GodOfWarPCPort SCUS_973.99`.
2.  **Identificação de Travamentos**: O jogo provavelmente travará ao tentar chamar uma função de hardware (Gráficos, Som, I/O) que ainda não tem uma implementação no PC.
3.  **Implementação de Stubs**: Criar implementações manuais no arquivo `ps2_recompiled_stubs.h` ou no runtime para as funções que o jogo exige para progredir.

### Fase C: Gráficos e Áudio
1.  **Mapeamento do GS (Graphics Synthesizer)**: Integrar as chamadas de desenho do PS2 com a API gráfica do PC (via Raylib ou OpenGL).
2.  **Sistema de Arquivos**: Garantir que o jogo consiga ler os arquivos de dados (.VPU, .WAD, etc.) do disco original.

---
**Nota para o Agente**: O código gerado é uma tradução literal. O segredo para o sucesso deste port está na qualidade da implementação do **Runtime** e na resolução paciente dos erros de execução que surgirão.
