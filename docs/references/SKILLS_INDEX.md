# Índice de skills e gatilhos

| Skill | Quando usar |
|---|---|
| `termuni-repo-discovery` | Auditar a base do Termuni antes de alterar arquitetura, descobrir linguagem, builds e se o motor é libghostty-internal, libghostty-vt ou runtime GTK do Ghostty. Acionar em primeiros passos, migração e bugs sem diagnóstico. |
| `gnome51-api-compat` | Conferir APIs de GTK 4.23, libadwaita 1.10 e GNOME 51 contra versões reais no host/SDK; planejar fallback de widgets e evitar símbolos inexistentes. Acionar ao usar APIs novas ou empacotar. |
| `gnome-native-architecture` | Planejar arquitetura nativa GNOME para Termuni com separação entre libadwaita, terminal engine, sessões, configurações e serviços GLib. Acionar em novas funcionalidades e refactors estruturais. |
| `gtk4-ui-composition` | Criar e revisar interfaces GTK 4 do Termuni: GtkBuilder, Blueprint quando adequado, ações, modelos, CSS mínimo e acessibilidade. Acionar em telas, menus, overlays e refatorações de UI. |
| `libadwaita-hig-termuni` | Aplicar as GNOME Human Interface Guidelines e libadwaita no Termuni: headerbar, abas, preferências, diálogos, adaptividade e identidade visual minimalista. Acionar para decisões de UX/visual. |
| `libghostty-api-integration` | Integrar e manter libghostty em Termuni sem confundir libghostty-internal, libghostty-vt e a aplicação GTK do Ghostty; conferir ABI e símbolos na revisão vendorizada. Acionar em bindings, dependências e upgrades. |
| `libghostty-gtk-rendering` | Diagnosticar renderização de superfícies Ghostty incorporadas ao GTK 4 no Termuni, incluindo GL, escala fracionária, redimensionamento, foco e ciclo de vida. Acionar em glitches, crash gráfico ou desempenho. |
| `terminal-pty-sessions` | Implementar ou depurar criação de PTY, shell, abas, splits e encerramento seguro de sessões no Termuni com libghostty. Acionar em nova aba, detach, cwd, hangups, resizing e limpeza de processos. |
| `terminal-input-shortcuts-ime` | Corrigir entrada e atalhos no Termuni, integrando mapeamento de teclas GTK e Ghostty com IME, PT-BR ABNT2, clipboard e seleção. Acionar em bugs de teclado e navegação. |
| `terminal-rich-graphics` | Investigar e implementar imagens e recursos ricos no Termuni com libghostty, distinguindo Kitty Graphics, SIXEL, hyperlinks OSC 8 e renderização de imagens no terminal. Acionar em imagens, previews e decoração de saída. |
| `terminal-shell-integration` | Aprimorar detecção de CWD, prompt marks, título, links, OSC e integração Bash/Zsh/Fish/Nushell no Termuni sem quebrar shell startup ou terminfo. Acionar em navegação inteligente e comando enriquecido. |
| `terminal-config-themes` | Projetar preferências nativas do Termuni e tradução segura para configuração de libghostty: fontes, paleta, opacidade, padding, tema, atalhos, GSettings e persistência. Acionar para tela Preferences. |
| `gnome-debug-perf` | Diagnosticar lentidão, congelamento, consumo de memória, jank de GTK/Ghostty e crash no Termuni com GTK Inspector, Sysprof, gdb e testes direcionados. Acionar em problemas de performance. |
| `gnome-a11y-i18n-qa` | Testar acessibilidade, localização pt-BR, teclado, contraste, foco, ícones simbólicos e qualidade visual do Termuni no GNOME. Acionar em release, telas novas e mudanças de interação. |
| `termuni-branding-upstream` | Fazer rebranding seguro de fork ou código baseado em Ptyxis/Ghostty para Termuni: app-id, ícone, desktop, recursos, strings e atribuições/licenças. Acionar em rename, limpeza e lançamento. |
| `termuni-command-ux` | Criar recursos de terminal moderno no Termuni: command blocks, resultados, search, previews e painel lateral, respeitando protocolos e dados confiáveis do libghostty. Acionar em UX rica e fluxos de produtividade. |
| `termuni-security-resilience` | Revisar segurança e estabilidade de terminal Termuni com libghostty: sequências não confiáveis, OSC, clipboard, links, arquivos e crashes. Acionar antes de lançar recursos ricos. |
