# Termac — requisitos e arquitetura

Atualizado em 09/11/2026. O estado dos marcos e suas evidências ficam no quadro **Estado atual** de [PLANO_IMPLEMENTACAO.md](PLANO_IMPLEMENTACAO.md); consulte-o antes de continuar a implementação.

## Produto

O Termac é um terminal para Linux com interface GNOME, sessões em abas e futura entrada de imagens para agentes de linha de comando. A prova inicial precisa abrir o shell do usuário em uma janela responsiva, preservar texto Unicode, cores e redimensionamento e, depois, sustentar aplicações TUI interativas. A aparência e as interações previstas estão em [DESIGN.md](DESIGN.md).

Os marcos seguintes incluem seleção, copiar e colar, scrollback, visão geral de abas, preferências, anexação de imagens para Codex, Claude Code, Hermes, OpenCode e Gemini, persistência de organização e acessibilidade. O plano descreve os critérios de aceite; a presença de uma ação ou tela no código não garante sua conclusão.

## Decisões técnicas

- C com GTK 4 e libadwaita; Meson como build e projeto aberto no GNOME Builder. Ambiente principal informado: Ubuntu 26.10 em Wayland.
- libghostty-vt faz parsing VT e fornece estado, células, estilos, cores e codificação de teclas. O Termac fornece janela, PTY, entrada e desenho.
- [subprojects/ghostty.wrap](subprojects/ghostty.wrap) fixa Ghostty em `f64f4aca2c29b554d111b36c3d946a9bddd159ff`. O build exige Zig `0.16.0`; o manifesto registra a URL oficial e SHA-256. A API C deve ser conferida na revisão fixada ao alterá-la. O exemplo oficial Ghostling foi a referência inicial para uso da API.
- O desenho atual usa `GtkDrawingArea`, Cairo e PangoCairo. Medições devem orientar qualquer mudança para GSK, OpenGL ou outro renderer.
- `development` usa `thiago.aciole.Termac.Devel`, schema e ícone próprios; `default` usa `thiago.aciole.Termac`. Recursos GtkBuilder mantêm o caminho `/thiago/aciole/termac`.
- A cópia de referência `/home/thiago/Downloads/termac.old` deriva do Ptyxis e usa `VteTerminal` e CMake. Serve como referência de estrutura e interface, com detalhes em [DESIGN.md](DESIGN.md); a sessão, entrada, renderização e recursos VTE precisam de adaptação para libghostty-vt. A cópia examinada não contém histórico Git.

## Código existente

| Local | Responsabilidade atual |
| --- | --- |
| [src/main.c](src/main.c) | Inicializa tradução e aplicação com o ID gerado pelo Meson. |
| [src/termac-application.c](src/termac-application.c) | Ciclo da aplicação, ações globais, preferências, paletas e GSettings. |
| [src/termac-window.c](src/termac-window.c) e [src/termac-window.ui](src/termac-window.ui) | Janela, cabeçalho, menu, abas, overview e ações de janela. |
| [src/termac-terminal.c](src/termac-terminal.c) | Widget, PTY e shell, I/O GLib, estado Ghostty, renderer Cairo/Pango e entrada/IME. As camadas de sessão e renderer ainda não são módulos separados. |
| [src/termac.css](src/termac.css) e [src/termac.gresource.xml](src/termac.gresource.xml) | Estilos e recursos incorporados. |
| [data/meson.build](data/meson.build) e [data/icons/meson.build](data/icons/meson.build) | Arquivos desktop, schema, serviço, metadados e ícones por perfil. |
| [subprojects/packagefiles/ghostty/meson.build](subprojects/packagefiles/ghostty/meson.build) | Integração Meson do build estático da biblioteca via Zig. |
| [thiago.aciole.termac.json](thiago.aciole.termac.json) | Manifesto Flatpak usado no fluxo de desenvolvimento do Builder; sua execução na IDE ainda precisa ser comprovada. |

## Fluxo de uma sessão

Cada `TermacTerminal` cria um shell via `forkpty`, integra o descritor não bloqueante ao main loop da GLib e alimenta libghostty-vt com a saída. A fila envia entrada e respostas VT ao PTY. O render state é lido no desenho GTK; redimensionamento atualiza a grade e o PTY. O evento de título atualiza a aba e o cabeçalho. O código contém controle de volume por iteração e limite de fila; a robustez sob carga ainda requer validação.

## Build e execução locais

Zig `0.16.0`, Meson, compilador C, GTK 4, libadwaita, PangoCairo e dependências do Ghostty são necessários. Com Zig disponível no `PATH`:

```sh
meson setup build -Dprofile=development  # somente se build/ ainda não existir
meson compile -C build
./build/src/termac
```

O Zig usado na sessão de 09/11/2026 foi `/tmp/termac-tools/zig-0.16.0/zig`; `/tmp` é temporário. Para o diretório existente, pode-se executar `PATH=/tmp/termac-tools/zig-0.16.0:$PATH meson compile -C build` enquanto esse arquivo existir. Para o perfil normal, configure um diretório de build separado com `-Dprofile=default`. O manifesto do Builder usa `-Dprofile=development` e o arquivo oficial do Zig; não há validação registrada de build e execução pela interface da IDE.

## Riscos e pontos de atenção

- O schema em execução é carregado por `BUILD_SCHEMA_DIR`, um caminho absoluto do diretório de build. Uma instalação executada sem essa árvore precisa ser corrigida e validada antes de ser considerada distribuível.
- O manifesto aponta para um repositório Git local em `file:///home/thiago/Projects/termac`. Em 09/11/2026 não havia commit inicial e os arquivos estavam não rastreados; verificar como o Builder obtém a árvore de trabalho antes de afirmar que o Flatpak reproduz as alterações locais.
- O encerramento atual usa `waitpid(..., 0)` no `dispose`, o que pode bloquear a interface se o processo filho não terminar. A fila de escrita tem limite de 1 MiB e rejeita dados excedentes; avaliar a política de pressão e erros.
- `TERM` é definido como `xterm-256color`; as capacidades anunciadas precisam ser confrontadas com o comportamento realmente implementado.
- Entrada composta, teclas, TUI, seleção, colagem, scrollback, Unicode complexo e desempenho exigem verificações específicas. A API e o desenho por célula não provam compatibilidade completa.
- As preferências visíveis cobrem esquema de cores, tamanho de fonte e paleta. As demais páginas e opções das referências continuam no plano.
