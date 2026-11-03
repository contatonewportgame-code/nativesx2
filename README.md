# NativeSX2 — Plataforma & Motor de Jogos PS2 Nativos para PC

[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-brightgreen.svg)]()
[![Language](https://img.shields.io/badge/C%2B%2B-20-blue.svg)]()
[![Graphics](https://img.shields.io/badge/Backend-OpenGL%20%2F%20Raylib-orange.svg)]()
[![Architecture](https://img.shields.io/badge/Arch-Universal%20x86--64-success.svg)]()

> **NativeSX2** é uma plataforma e motor de execução nativa para portas e recompilações estáticas de jogos de PlayStation 2 em computadores modernos (Windows 10/11 e Linux). O projeto combina pesquisa avançada de engenharia reversa de baixo nível, otimizações gráficas de tempo real e uma interface amigável completa para jogadores e desenvolvedores.

---

## 🌟 Principais Recursos do NativeSX2

- 🎮 **Central Gráfica Multiplataforma (Windows e Linux):** Interface limpa desenvolvida com renderização vetorial TrueType e suporte multilíngue em tempo real (Português do Brasil e Inglês).
- ⚡ **Modos Rápidos de Desempenho (1 Clique):**
  - *Modo PC Fraco / Notebook:* Renderização 1x nativa, sombras rápidas (`GOW_GS_FAST_SHADOWS`) e janela otimizada para chips gráficos integrados (Intel HD) e GPUs modestas.
  - *Modo PC Gamer:* Resolução HD escalonada (2x / 3x), sombras originais e tela cheia sem bordas a 60 FPS cravados.
- 🚀 **Otimizações do Motor Gráfico GS:**
  - `PresentCache` e modo `FastShadows`: Reduz centenas de chamadas custosas a `glTextureBarrier()` em passagens de sombra stencil (`COLCLAMP=0`) e elimina leituras lentas de framebuffer (`glReadPixels`).
  - Otimização do interpretador VU1: Eliminação de sobrecarga de hashing em tempo real, garantindo fluidez constante.
- 💿 **Extrator ISO9660 em C++ Puro Integrado:**
  - O executável lê diretamente imagens de disco `.iso` e extrai os cabeçalhos (`SYSTEM.CNF`), TOC e módulos `.IRX` em menos de **0.2 segundos**, dispensando programas externos como 7-Zip.
- 📂 **Modo Direto de 2 Cliques na .ISO:**
  - Integração nativa com o Windows Explorer e gerenciadores de arquivo Linux. Ao dar 2 cliques em qualquer arquivo `.iso` compatível, o jogo inicia diretamente com as preferências salvas.
- 💾 **Gerenciador de Memory Card Virtual (8 MB):**
  - Criação automática do cartão de memória virtual (`Mcd001.ps2`), com funções de **Backup** e **Restauração** em 1 clique na interface.
- 🕹️ **Remapeamento Dinâmico de Controles e Teclado:**
  - Configuração em tempo de execução para todos os botões do DualShock 2, compatível com controles USB, PlayStation (PS3/PS4/PS5), Xbox, controles genéricos Bluetooth e Teclado completo.

---

## 🏛️ Créditos, Autoria e Atribuições Éticas

O NativeSX2 é construído sobre a cooperação aberta entre a pesquisa independente da **NewPortGame** e o trabalho pioneiro da comunidade open-source internacional:

### 1. Pesquisa, Engenharia e Desenvolvimento — NewPortGame
* **Cristiano Mariano (NewPortGame)** — *Fundador, Pesquisa Principal e Engenharia de Plataforma*
  * **Cadeia Causal de Boot EE $\leftrightarrow$ IOP:** Mapeamento estático e dinâmico da sequência de inicialização legítima, sincronização entre Thread Principal e Thread 2 (`0x002947c8`), publicação guest e liberação do semáforo `WaitSema(3)` sem injeções artificiais de host.
  * **Protocolo de Comunicação SIF RPC:** Descoberta e resolução do fluxo `SifBindRpc` (0x61), `SifCallRpc` (0x62), buffer de pacotes RPC DMAC5 e sinalização da flag `CMDINIT` (`0x20000`) no registrador `SMFLAG` via `SifGetReg(4)`.
  * **Auditoria Estrutural do IOPRP300.IMG:** Desmontagem e identificação correta dos módulos do IOP (identificando bibliotecas internas `LoadModuleByEE` em `loadelf` e `Timer_Manager` em `intrman`).
  * **Correções de Semântica de Instruções MIPS R5900:**
    - Registrador `COP0.Status`: Correção da semântica de `EI/DI` restaurando o controle dos bits `IE|EIE` (`0x10001`) e eliminação do hack que limpava `EIE` globalmente no `sync`.
    - Instruções Quadword `LQ`/`SQ`: Correção no emissor para uso de `r_hi[rt]`, evitando corrupção de registradores no canal VIF1 (`0x10005000`).
  * **Resolução dos 430+ Bloqueios de Boot:** Mais de 430 passos iterativos de depuração de máquina de estados (`c838`), chamadas de disco (`sceCdRead`) e overlays DVP.
  * **Desenvolvimento da Plataforma NativeSX2:** Criação da interface em Raylib, extrator nativo ISO9660 em C++, gerenciador de saves, pipeline de empacotamento portátil e instaladores de sistema.

### 2. Projetos e Colaboradores da Comunidade (Upstream)
* **ran-j e colaboradores — Projeto PS2Recomp** ([GitHub](https://github.com/ran-j/PS2Recomp))
  * Desenvolvimento da ferramenta de recompilação estática de instruções MIPS R5900 para C++ e arquitetura base do runtime de execução (GPL-3.0).
* **Waffle565 e colaboradores — God-Of-War-Native-Port & GoW Recomp** ([GitHub](https://github.com/Waffle565/God-Of-War-Native-Port))
  * Desenvolvimento inicial do runtime nativo para God of War (sintetizador gráfico GS, VU1, IOP, áudio 989snd, DMA e leitor de disco) e ferramentas auxiliares.
* **Vnnc1 e colaboradores — gow-native-pc & Stack de Otimizações** ([GitHub](https://github.com/Vnnc1/gow-native-pc))
  * Pesquisa e calibração de métricas para 60 FPS, benchmarks em hardware heterogêneo, patch stack de otimizações (VU1 fast mode, áudio exponencial SPU2 por deslocamento aritmético, descarte de pacotes obsoletos em cutscenes MPEG) e documentação técnica de suporte.
* **Equipe e Desenvolvedores do PCSX2** ([pcsx2.net](https://pcsx2.net) / [GitHub](https://github.com/PCSX2/pcsx2))
  * Referência do sintetizador gráfico GS, interpretador de referência do Emotion Engine (`ee_ref`) e operações das unidades vetoriais (`vu_pcsx2`) sob licença GPL-3.0+.
* **Taylor N. Albarnaz (LightVelox) — Fork do Runtime de Shadow of the Colossus** ([GitHub](https://github.com/LightVelox/PS2Recomp))
  * Backend gráfico GS em OpenGL, fila de comandos gráficos e correções de precisão do processador EE (FPU, desvios de 64 bits, macros VU0 e cadeias DMA longas).
* **thiagordsfx — Projeto Urban Reign Recomp** ([GitHub](https://github.com/thiagordsfx/urban-reign-recomp))
  * Referência compartilhada para integração do GS e sincronismo vertical de campo (VSync).
* **Ramon Santamaria (Raylib) e Omar Cornut (ImGui)**
  * Bibliotecas de janela gráfica multiplataforma e controles de interface.

---

## 📁 Estrutura do Repositório

```text
nativesx2/
├── assets/                 # Ícones de alta resolução (.ico) e fontes vetoriais TTF
│   ├── nativesx2.ico       # Ícone oficial da plataforma NativeSX2
│   ├── nativesx2_iso.ico   # Ícone de disco PS2 para associação com arquivos .ISO
│   └── nativesx2_font*.ttf # Tipografia vetorial anti-aliased da interface
├── docs/                   # Documentação do usuário e licença
│   ├── LEIAME_E_CREDITOS.txt
│   └── research/           # Registro científico de auditorias, notas de rodadas e handoffs
│       ├── AUDITORIA_CONCLUSOES_2026-09-01.md
│       ├── HANDOFF_2026-09-02_IOP_INTEGRATION.md
│       ├── investigation_notes.md
│       └── COMPATIBILITY_MATRIX.md
├── installer/              # Scripts de instalação e empacotamento
│   ├── inno_setup_script.iss   # Script do instalador assistido oficial (Windows)
│   ├── INSTALAR_1_CLIQUE.cmd   # Script de configuração portátil de 1 clique
│   └── GERAR_INSTALADOR_OFICIAL.cmd
├── src/                    # Código-fonte da aplicação e subsistemas
│   ├── nativesx2_launcher.cpp  # Interface gráfica e extrator de ISO integrado
│   ├── ps2_pad.cpp             # Mapeamento dinâmico de teclado e controles USB
│   └── gow_overrides.cpp       # Overrides de tempo de execução e otimizações
└── tools/                  # Ferramentas analíticas e scanners MIPS em Python
    ├── audit_guest_address_xrefs.py
    ├── scan_guest_pointer_xrefs.py
    ├── scan_mips_string_xrefs.py
    ├── iso_extract.py
    └── embed_icon.py
```

---

## 🛠️ Como Compilar

### Windows (MSVC 2022 / x64 Universal)
1. Instale o Visual Studio 2022 com a carga de trabalho de **Desenvolvimento para Desktop com C++** e a biblioteca **Raylib**.
2. Abra o terminal x64 Native Tools Command Prompt.
3. Para compilar a central gráfica:
   ```cmd
   cl /nologo /MD /O2 /EHsc /utf-8 /std:c++20 src/nativesx2_launcher.cpp /I"caminho/para/raylib/include" /Fe:NativeSX2.exe "caminho/para/raylib/raylib.lib" opengl32.lib gdi32.lib winmm.lib user32.lib shell32.lib advapi32.lib /link /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup
   ```
4. Para embutir os recursos de ícone no executável:
   ```cmd
   python tools/embed_icon.py NativeSX2.exe assets/nativesx2.ico
   ```

### Linux (GCC / Clang)
1. Instale os pacotes de desenvolvimento:
   ```bash
   sudo apt install build-essential cmake libraylib-dev libgl1-mesa-dev
   ```
2. Compile a aplicação:
   ```bash
   g++ -std=c++20 -O2 src/nativesx2_launcher.cpp -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -o NativeSX2
   ```

---

## ⚖️ Aviso Legal e Licença

Este software é um motor e inicializador nativo independente distribuído sob a **Licença GNU GPL-3.0**.

- **Nenhum arquivo protegido por direitos autorais, BIOS proprietária ou imagem de disco (.ISO) de qualquer jogo é distribuído com este repositório ou com o software.**
- O usuário final deve fornecer sua própria cópia legítima extraída de discos originais adquiridos legalmente.
- PlayStation, PlayStation 2, PS2, DualShock e títulos de jogos mencionados são marcas registradas de seus respectivos detentores (Sony Interactive Entertainment Inc., etc.). Este projeto não tem relação, patrocínio ou afiliação oficial com as referidas marcas.
