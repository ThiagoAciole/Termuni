# Fontes rastreáveis — compilado em 2026-11-09

## Links fornecidos e interpretação
- [GNOME 51 para desenvolvedores](https://release.gnome.org/51/developers/) — GTK 4.23 e libadwaita 1.10, recursos específicos; não garante pacotes locais.
- [Documentação GNOME](https://developer.gnome.org/documentation/) — hierarquia para tutoriais, APIs, acessibilidade, ferramentas, Guidelines.
- [Componentes GNOME](https://developer.gnome.org/components/) — inventário de ferramentas e bibliotecas como GTK, Blueprint, libadwaita, VTE (apenas comparativo; Termac usa libghostty).
- [Linguagens GNOME](https://developer.gnome.org/documentation/introduction/languages.html) — C, C++, GJS, Python, Rust, Vala etc. Zig não está no quadro principal mas o Ghostty usa Zig e GTK.

## Links adicionais essenciais
- [GNOME HIG](https://developer.gnome.org/hig/) — padrões visuais/interação.
- [API GTK](https://docs.gtk.org/gtk4/) — símbolos e recursos da versão 4.
- [GTK Inspector e Sysprof](https://docs.gtk.org/gtk4/running.html) — diagnóstico de UI.
- [API libadwaita 1.10](https://gnome.pages.gitlab.gnome.org/libadwaita/doc/1.10/) — consultar se versão existir no runtime.
- [Libadwaita: migrating to breakpoints](https://gnome.pages.gitlab.gnome.org/libadwaita/doc/1.10/migrating-to-breakpoints.html) — toolbar view e layout adaptativo.
- [Ghostty overview](https://ghostty.org/docs/about) — arquitetura, GTK/Linux e função de libghostty.
- [Ghostty source build](https://ghostty.org/docs/install/build) — dependências Linux, Zig e recursos.
- [Ghostty embedded internal header](https://github.com/ghostty-org/ghostty/blob/main/include/ghostty.h) — API interna, não é API estável de integração para terceiros.
- [libghostty-vt header](https://github.com/ghostty-org/ghostty/blob/main/include/ghostty/vt.h) — parsing/estado de terminal, explicitamente experimental.
- [Ghostty source GTK](https://github.com/ghostty-org/ghostty/tree/main/src/apprt/gtk) — integração GTK original.
- [Ghostty AGENTS.md](https://github.com/ghostty-org/ghostty/blob/main/AGENTS.md) — comandos upstream (não presumir compatibilidade Termac).
- [Ghostty shell integration](https://ghostty.org/docs/features/shell-integration) — CWD, prompt markers, terminfo, ssh.
- [Ghostty configuration](https://ghostty.org/docs/config) — reload e arquivos.
- [Ghostty config reference](https://ghostty.org/docs/config/reference) — opções/fontes/input.
- [Ghostty keybindings](https://ghostty.org/docs/config/keybind) — mapeamento de comandos.
- [Discussão API interna](https://github.com/ghostty-org/ghostty/discussions/14360) — upstream esclarece diferenças de escopo.
- [Agent Skills spec](https://agentskills.io/specification) — formato SKILL.md.
- [Codex Build Skills](https://learn.chatgpt.com/docs/build-skills) — localização de `.agents/skills`.

## Política de atualização
Nunca afirmar que links na branch main correspondem à revisão do Termac. Sempre checar primeiro a versão git/tag e headers efetivos presentes no repositório antes de implementar.
