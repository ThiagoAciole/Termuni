# Desenvolvimento do Termac

Execute comandos na raiz que contém `meson.build` e `AGENTS.md`. Antes de editar, confira `git status --short`, [o quadro vigente](../PLANO_IMPLEMENTACAO.md) e [o roadmap](ROADMAP.md).

## Mapa do repositório

| Local | Conteúdo |
| --- | --- |
| `src/` | Aplicação C, widgets/ações, motor/PTY/entrada/desenho, adaptadores, GtkBuilder/CSS/GResources. Ownership e limites em [arquitetura](../PDR.Arquiteture.md). |
| `data/` | Desktop, GSettings, serviço D-Bus, AppStream e ícones por perfil. |
| `assets/` | Artes originais dos seletores de tema; não sobrescrever fontes para gerar derivados. |
| `po/` | Catálogo gettext ativo, template e lista de idiomas. Backups antigos estão em `docs/history/backups/`. |
| `tests/` | Testes GLib/GTK, fixtures sintéticas de agentes e driver real opt-in. |
| `tools/` | [Ferramentas locais](../tools/README.md); use os procedimentos documentados antes de scripts que encerram sessões ou acionam modelos. |
| `subprojects/` | Wrap/revisão e integração Meson do Ghostty. `ghostty/` e `packagecache/` são dependências baixadas e ignoradas. |
| `docs/` | Índice, roadmap, checklists, relatórios, planos e histórico. |
| `build/`, `build-*/`, `.flatpak-builder/`, `.superpowers/` | Artefatos locais ignorados; podem conter evidência de trabalho anterior. Ignorar no Git não autoriza apagar. |

## Toolchain e perfis

- Zig **0.16.0** é verificado pelo Meson. [O wrap](../subprojects/ghostty.wrap) fixa Ghostty em `f64f4aca2c29b554d111b36c3d946a9bddd159ff`.
- Dependências declaradas em `meson.build`/`src/meson.build`: C, GTK 4, libadwaita ≥ 1.8, GLib/GIO, PangoCairo e util do sistema. Testes UI também usam X11/XTest via Xvfb.
- `development`: `thiago.aciole.Termac.Devel`; `default`: `thiago.aciole.Termac`. Ambos têm schema/ícone próprios; o caminho de GResources continua `/thiago/aciole/termac`.

```sh
# Primeira configuração; não repetir setup num diretório já configurado.
meson setup build -Dprofile=development
meson compile -C build

# Perfil normal separado; não muda a configuração do build/.
meson setup build-default -Dprofile=default
meson compile -C build-default
```

`tools/dev.sh` compila e inicia somente se não houver processo Termac. Carrega traduções locais e grava log em `~/.cache/termac/dev.log` (ou `TERMAC_DEV_LOG`). **`--restart` encerra shells/abas existentes**; informe o usuário antes de usá-lo. Não use `G_DEBUG=fatal-warnings` para manter o app pessoal aberto.

## Testes e evidência

```sh
# Rápidos, sem abrir a interface:
meson test -C build util vt attachments settings agent agent-names-contained --print-errorlogs

# Suíte completa, incluindo UI normal e HiDPI:
meson test -C build --print-errorlogs
```

A UI usa Xvfb/X11 e diretórios XDG próprios. A execução comum pula os auxiliares de CLIs reais, sandbox Flatpak e capturas. Não comprova GNOME Builder, compositor Wayland, Orca ou recebimento de notificações do desktop. Para esses cenários use as [checklists](README.md#checklists-de-aceite).

Alguns casos UI antigos compartilham estado e não funcionam isolados; M01 do [roadmap](ROADMAP.md) identifica a correção necessária. Mesmo com a suíte verde, não alegue que `large-output.png` prova 30 mil linhas antes de reparar esse cenário.

Para capturas reproduzíveis:

```sh
mkdir -p /tmp/termac-shots
TERMAC_UI_SCREENSHOTS=/tmp/termac-shots GSETTINGS_BACKEND=memory \
  GDK_BACKEND=x11 xvfb-run -a build/tests/test-ui -p /ui/visual-screens
```

A busca tem captura própria em `/ui/search-highlight`; use a execução completa para reproduzir a sequência original de todas as capturas. O procedimento e a inspeção anterior estão na [revisão visual](checklists/visual-review.md).

Valgrind: [tools/valgrind-churn.sh](../tools/valgrind-churn.sh) mantém build separado com SIMD desligado, pois o motor fixado gera instruções que o Valgrind instalado não interpreta. Compare tamanhos de churn e frames do app; não chame todo cache de biblioteca de vazamento. As supressões ficam em `tools/valgrind.supp`.

## Flatpak e GNOME Builder

O manifesto [thiago.aciole.termac.json](../thiago.aciole.termac.json) usa GNOME 50 e development. Abra a raiz no Builder, selecione o manifesto e registre o resultado na [checklist GNOME](checklists/gnome-manual.md). O procedimento CLI e o driver de teste do PTY host estão nessa mesma checklist.

O shell roda intencionalmente no host. Não acrescente rede/acesso geral à home ao processo gráfico para resolver diferenças de contexto; confira [as decisões técnicas](../PDR.Arquiteture.md).

## Traduções e documentação

```sh
# Atualiza o template no diretório po/:
meson compile -C build termac-pot
msgmerge --update --backup=none --no-fuzzy-matching po/pt_BR.po po/termac.pot
msgfmt --check --statistics -o /tmp/termac-pt_BR.mo po/pt_BR.po
```

Depois de preencher traduções novas, confirme zero fuzzy/pendentes. Mudanças de documentação não exigem refazer toda a suíte gráfica: confira links/caminhos, preservação de arquivos movidos e build quando referências de build mudarem. Para mudanças funcionais, execute os testes correspondentes e a suíte necessária.

Atualize [PLANO_IMPLEMENTACAO.md](../PLANO_IMPLEMENTACAO.md) e o item do [roadmap](ROADMAP.md); resultados longos e datados vão em `docs/reports/`. Preserve o histórico e distinga implementado, testado automaticamente e confirmado no desktop real.
