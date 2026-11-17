# Desenvolvimento do Termuni

Execute comandos na raiz que contém `meson.build` e `AGENTS.md`. Antes de editar, confira `git status --short`, [o quadro vigente](PLANO_IMPLEMENTACAO.md) e [o roadmap](ROADMAP.md).

## Mapa do repositório

O mapa visual e as regras para novos arquivos estão em [Estrutura do repositório](estrutura.md).

| Local | Conteúdo |
| --- | --- |
| `src/` | Aplicação C, widgets/ações, motor/PTY/entrada/desenho, adaptadores, GtkBuilder/CSS/GResources. Ownership e limites em [arquitetura](architecture/PDR.Arquiteture.md). |
| `data/` | Desktop, GSettings, serviço D-Bus, AppStream e ícones por perfil. |
| `assets/` | Artes originais dos seletores de tema; não sobrescrever fontes para gerar derivados. |
| `po/` | Catálogo gettext ativo, template e lista de idiomas. Backups antigos estão em `docs/history/backups/`. |
| `tests/` | Testes GLib/GTK, fixtures sintéticas de agentes e driver real opt-in. |
| `tools/` | [Ferramentas locais](../tools/README.md); use os procedimentos documentados antes de scripts que encerram sessões ou acionam modelos. |
| `subprojects/` | Wrap/revisão e integração Meson do Ghostty. `ghostty/` e `packagecache/` são dependências baixadas e ignoradas. |
| `docs/` | Índice, roadmap, checklists, relatórios, planos e histórico. |
| `build/` | Builds atuais, pacotes locais, caches e arquivos antigos preservados; veja o [mapa da estrutura](estrutura.md). Ignorar no Git não autoriza apagar. |

## Toolchain e perfis

- Zig **0.16.0** é verificado pelo Meson. [O wrap](../subprojects/ghostty.wrap) fixa Ghostty em `f64f4aca2c29b554d111b36c3d946a9bddd159ff`.
- Dependências declaradas em `meson.build`/`src/meson.build`: C, GTK ≥ 4.22, libadwaita ≥ 1.8, GLib/GIO, PangoCairo e util do sistema. Testes UI também usam X11/XTest via Xvfb.
- `development`: `io.github.ThiagoAciole.Termuni.Devel`; `default`: `io.github.ThiagoAciole.Termuni`. Ambos têm schema/ícone próprios; o caminho de GResources continua `/io/github/thiagoaciole/termuni`.

```sh
# Primeira configuração; não repetir setup num diretório já configurado.
meson setup build/development -Dprofile=development
meson compile -C build/development

# Perfil normal separado; não muda a configuração do build/.
meson setup build/default -Dprofile=default
meson compile -C build/default
```

`tools/dev.sh` compila e inicia somente se o executável development desta raiz ainda não estiver aberto. Confere o perfil e a identidade em `/proc/PID/exe`, preservando instâncias de outros checkouts/perfis. Carrega traduções locais e grava log em `~/.cache/termuni/dev.log` (ou `TERMUNI_DEV_LOG`). **`--restart` informa os PIDs e encerra somente os shells/abas desse executável**; informe o usuário antes de usá-lo. Não use `G_DEBUG=fatal-warnings` para manter o app pessoal aberto.

## Testes e evidência

```sh
# Rápidos, sem abrir a interface:
meson test -C build/development util vt attachments settings agent agent-names-contained --print-errorlogs

# Suíte completa, incluindo UI normal e HiDPI:
meson test -C build/development --print-errorlogs
```

A UI usa Xvfb/X11 e diretórios XDG próprios. A execução comum pula os auxiliares de CLIs reais e capturas. Não comprova GNOME Builder, compositor Wayland, Orca ou recebimento de notificações do desktop. Para esses cenários use as [checklists](README.md#checklists-de-aceite).

Alguns casos UI antigos compartilham estado e não funcionam isolados; M01 do [roadmap](ROADMAP.md) identifica a correção necessária. Mesmo com a suíte verde, não alegue que `large-output.png` prova 30 mil linhas antes de reparar esse cenário.

Para capturas reproduzíveis:

```sh
mkdir -p /tmp/termuni-shots
TERMUNI_UI_SCREENSHOTS=/tmp/termuni-shots GSETTINGS_BACKEND=memory \
  GDK_BACKEND=x11 xvfb-run -a build/development/tests/test-ui -p /ui/visual-screens
```

A busca tem captura própria em `/ui/search-highlight`; use a execução completa para reproduzir a sequência original de todas as capturas. O procedimento e a inspeção anterior estão na [revisão visual](checklists/visual-review.md).

Valgrind: [tools/valgrind-churn.sh](../tools/valgrind-churn.sh) mantém build separado com SIMD desligado, pois o motor fixado gera instruções que o Valgrind instalado não interpreta. Compare tamanhos de churn e frames do app; não chame todo cache de biblioteca de vazamento. As supressões ficam em `tools/valgrind.supp`.

## GNOME Builder nativo e pacote `.deb`

Abra a raiz no GNOME Builder como projeto Meson e configure um build `development` separado. O app abre sem sandbox: o PTY, o shell e os agentes executam localmente com as permissões normais do usuário. A [checklist GNOME](checklists/gnome-manual.md) registra a validação pela IDE, a instalação Debian e o aceite visual.

Para compilar o perfil instalável e gerar um `.deb` local:

```sh
python3 tools/build-deb.py
sudo apt install ./build/packages/io.github.ThiagoAciole.Termuni_*.deb
```

O empacotador configura o perfil `default`, prepara o conteúdo em diretório temporário, calcula dependências dinâmicas e inclui lançador, schema, serviço D-Bus, traduções e ícone.

## Traduções e documentação

```sh
# Atualiza o template no diretório po/:
meson compile -C build/development termuni-pot
msgmerge --update --backup=none --no-fuzzy-matching po/pt_BR.po po/termuni.pot
msgfmt --check --statistics -o /tmp/termuni-pt_BR.mo po/pt_BR.po
```

Depois de preencher traduções novas, confirme zero fuzzy/pendentes. Mudanças de documentação não exigem refazer toda a suíte gráfica: confira links/caminhos, preservação de arquivos movidos e build quando referências de build mudarem. Para mudanças funcionais, execute os testes correspondentes e a suíte necessária.

Atualize [PLANO_IMPLEMENTACAO.md](PLANO_IMPLEMENTACAO.md) e o item do [roadmap](ROADMAP.md); resultados longos e datados vão em `docs/reports/`. Preserve o histórico e distinga implementado, testado automaticamente e confirmado no desktop real.

## Consolidação e CI

`bash tools/ci-check.sh development` e `bash tools/ci-check.sh default` executam o mesmo caminho do job nativo: build, suítes normal/HiDPI, traduções e instalação em DESTDIR dentro do próprio build. `TERMUNI_CI_BUILD=build/ci/development` permite usar uma configuração existente. A instalação de teste não modifica `/usr`.

`.github/workflows/ci.yml` compila e verifica os perfis development/default em Fedora 44 (GTK 4.22+). Zig usa a URL/hash fixados em `tools/zig-toolchain.json`; Ghostty continua no wrap fixado. O workflow publica logs de CI, não releases; a geração do `.deb` ocorre com `tools/build-deb.py`. A execução remota depende de o repositório estar hospedado no GitHub.

A ponte acessível tem teste real com buses D-Bus privados, sem conexão ao bus do desktop:

```sh
/usr/bin/python3 tests/check-accessible-atspi.py --binary build/development/tests/test-ui
```

Requer Python GI/Atspi, Xvfb, `at-spi2-registryd` e `dbus-daemon`; `--dbus-daemon CAMINHO` permite selecionar uma instalação de teste. Isso verifica texto/caret/seleção/eventos, sem substituir o aceite do Orca.

O benchmark é opt-in e cria apenas sessões próprias:

```sh
python3 tools/benchmark.py --samples 3 --lines 3000 --output build/benchmark.json
```

São 1/5/10 abas, arquivos de saída completos conferidos, busca, resize, CPU ociosa e RSS do frontend. O JSON descreve o ambiente e os tempos do driver. Cairo/Xvfb não mede aceleração GPU ou latência física Wayland. Não execute a matriz junto de builds pesados se quiser comparar desempenho.

Para verificar também a leitura e geração de texto pelo Orca instalado (testado com 51.0), acrescentar `--orca` a `tests/check-accessible-atspi.py`. Não reproduz áudio. O runner sempre cria Xvfb próprio, mesmo com DISPLAY herdado.
