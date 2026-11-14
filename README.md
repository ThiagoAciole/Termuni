# Termac

Terminal Linux em C, GTK 4 e libadwaita, com motor **libghostty-vt**, sessões independentes em abas e anexos de imagem para Claude Code e Codex CLI.

O MVP inclui busca com realce, seleção/copiar/colar, overview de abas, paletas, atalhos editáveis, restauração de pastas e organização e notificações opt-in. Está em desenvolvimento: o aceite no GNOME/Wayland real ainda tem itens abertos.

## Comece aqui

- [Estado atual e próximo incremento](PLANO_IMPLEMENTACAO.md)
- [Melhorias priorizadas e critérios de conclusão](docs/ROADMAP.md)
- [Índice da documentação](docs/README.md)
- [Desenvolvimento, build e testes](docs/development.md)
- [Requisitos e arquitetura](PDR.Arquiteture.md), [design](DESIGN.md) e [orientações para agentes](AGENTS.md)

## Build local

Pré-requisitos: compilador C, Meson/Ninja, pkg-config, GTK **≥ 4.22**, libadwaita **≥ 1.8**, GLib/GIO, PangoCairo e **Zig 0.16.0** no PATH. A revisão de Ghostty fica fixada em [subprojects/ghostty.wrap](subprojects/ghostty.wrap); o primeiro build baixa o código e pacotes Zig e precisa de rede.

Primeira configuração:

```sh
zig version
meson setup build -Dprofile=development
```

Compilar e executar um build já configurado:

```sh
meson compile -C build
build/src/termac
```

Para usar as traduções locais e evitar abrir outra instância desnecessária, veja [tools/dev.sh](tools/dev.sh). O modo `--restart` encerra shells e abas existentes e exige cuidado com trabalho aberto.

## Testes

```sh
meson test -C build --print-errorlogs
```

As suítes de interface usam Xvfb/X11 quando disponível e cobrem também HiDPI. Testes que precisam de contas reais de agentes, sandbox Flatpak ou capturas são opt-in; a suíte comum não chama modelos. Procedimentos e limites em [desenvolvimento](docs/development.md) e nas [checklists](docs/README.md#checklists-de-aceite).

## GNOME Builder e perfis

Abra esta raiz como projeto Meson e selecione [thiago.aciole.termac.json](thiago.aciole.termac.json). O manifesto usa GNOME 50, Zig 0.16.0 e o perfil `development`, ID `thiago.aciole.Termac.Devel`.

O perfil `default` usa `thiago.aciole.Termac`; configure-o em outro diretório (`build-default`). Não troque o perfil de um build em uso. Compilação/exportação Flatpak CLI já foram registradas; execução pela interface do Builder continua pendente na [checklist GNOME](docs/checklists/gnome-manual.md).

No Flatpak, por decisão do projeto, o shell e os agentes executam no host via `flatpak-spawn`; consulte as [decisões técnicas](PDR.Arquiteture.md#decisões-técnicas).

Licença: [GPL-3.0-or-later](COPYING). As artes e avisos existentes são preservados.
