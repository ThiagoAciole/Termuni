# Termuni

**Um terminal nativo para GNOME, feito para trabalhar com várias sessões.**

O Termuni combina GTK 4, libadwaita e `libghostty-vt` em um aplicativo Linux escrito em C. Cada aba mantém sua própria sessão de shell. O projeto também oferece busca no terminal, restauração de abas e envio de imagens para Claude Code e Codex CLI.

> **Estado:** em desenvolvimento. Build e testes automatizados foram executados; validação no GNOME/Wayland real ainda está em andamento. Consulte o [estado atual](docs/PLANO_IMPLEMENTACAO.md) antes de interpretar os recursos como prontos para lançamento.

## O que você encontra

- Sessões de shell independentes em abas, com overview e restauração de pastas.
- Busca com realce, seleção, copiar e colar.
- Paletas, fontes e atalhos configuráveis.
- Anexos de imagem para Claude Code e Codex CLI, quando instalados.
- Interface GTK 4/libadwaita integrada ao GNOME.
- Perfis separados para desenvolvimento (`.Devel`) e uso normal.

## Começar a desenvolver

### Requisitos

Linux com compilador C, Meson, Ninja, pkg-config, GTK **4.22 ou superior**, libadwaita **1.8 ou superior**, GLib/GIO, PangoCairo e **Zig 0.16.0**. A revisão do `libghostty-vt` é fixada pelo wrap do Meson. O primeiro build baixa dependências e precisa de acesso à rede.

### Compilar e executar

```sh
zig version
meson setup build/development -Dprofile=development
meson compile -C build/development
build/development/src/termuni
```

Para iniciar com as traduções locais, consulte [`tools/dev.sh`](tools/dev.sh). O perfil de desenvolvimento usa o ID `io.github.ThiagoAciole.Termuni.Devel`; o perfil `default` usa `io.github.ThiagoAciole.Termuni` e deve ser configurado em outro diretório:

```sh
meson setup build/default -Dprofile=default
meson compile -C build/default
```

O projeto também pode ser aberto como projeto Meson no GNOME Builder. Veja [Desenvolvimento](docs/development.md) para dependências, testes, traduções, CI e empacotamento `.deb`.

## Testes

```sh
meson test -C build/development --print-errorlogs
```

A suíte inclui testes de GTK em Xvfb e HiDPI. Testes que dependem de contas ou CLIs reais são opt-in. Uma suíte aprovada não substitui a validação manual no GNOME Builder, Wayland, IME, acessibilidade e notificações. Os resultados e pendências ficam no [plano](docs/PLANO_IMPLEMENTACAO.md) e nas [checklists](docs/README.md#checklists-de-aceite).

## Mapa do projeto

| Caminho | Conteúdo |
| --- | --- |
| [`src/`](src/) | Aplicativo C, interface, terminal, PTY e integração com agentes. |
| [`data/`](data/) | Ícones, schema GSettings, serviço D-Bus e metadados do aplicativo. |
| [`tests/`](tests/) | Testes automatizados, fixtures e cenários opt-in. |
| [`tools/`](tools/) | Scripts de desenvolvimento, CI e empacotamento. |
| [`docs/`](docs/README.md) | Roadmap, arquitetura, design, checklists e relatórios. [Mapa da estrutura](docs/estrutura.md). |
| [`subprojects/`](subprojects/) | Wrap e integração Meson da revisão fixada do Ghostty. |
| [`po/`](po/) | Traduções gettext. |

Builds, pacotes gerados e caches ficam fora do controle de versão. Veja [`.gitignore`](.gitignore) e as [orientações para contribuir](AGENTS.md) antes de alterar a organização ou os arquivos do projeto.

## Documentação

- [Plano e estado atual](docs/PLANO_IMPLEMENTACAO.md)
- [Roadmap e critérios de conclusão](docs/ROADMAP.md)
- [Índice de documentação](docs/README.md)
- [Requisitos e arquitetura](docs/architecture/PDR.Arquiteture.md)
- [Design e referências visuais](docs/design/DESIGN.md)
- [Desenvolvimento, testes e CI](docs/development.md)

## Licença

Termuni é distribuído sob a licença [GNU GPL v3 ou posterior](COPYING). Avisos de copyright e proveniência dos recursos devem ser preservados.
