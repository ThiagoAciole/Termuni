# Ferramentas do Termac

Executar a partir da raiz. Build, perfis e suíte padrão estão em [docs/development.md](../docs/development.md).

| Ferramenta | Uso | Efeito / limite |
| --- | --- | --- |
| `dev.sh` | Compilar e abrir desenvolvimento, com traduções locais | Verifica o executável development desta raiz em `/proc`, preservando outros perfis/checkouts; `--restart` avisa os PIDs e encerra somente suas sessões. Recusa perfil default, partida falha e duplicação após timeout. Não executar esse modo sem informar antes. |
| `build-deb.py [--builddir DIR] [--output DIR]` | Compilar perfil `default` e gerar pacote Debian instalável | Requer Zig 0.16.0, Meson, Ninja e `dpkg-dev`; saída local em `dist/`. Instale com `sudo apt install ./dist/thiago.aciole.termac_*.deb`. |
| `inspect_termac.py --root . --json` | Inventário heurístico do frontend e versões locais | Somente leitura. Ignora builds/caches e Ghostty baixado, mas mantém a integração `subprojects/packagefiles/ghostty`. Não comprova ABI/linkagem nem suporte a funcionalidades do motor. |
| `prepare-icon.py INPUT OUTPUT` | Gerar canvas quadrado para instalação | Chamado pelo Meson; não sobrescrever o SVG fonte. |
| `check-agent-boundary.py src` | Detectar nomes de CLIs fora dos adaptadores | Já integra `meson test` como `agent-names-contained`. |
| `valgrind-churn.sh [rounds]` | Abrir/fechar abas sob Valgrind | Cria/usa `build-valgrind` sem SIMD e display Xvfb próprio; consultar o script e `valgrind.supp` antes de interpretar o resultado. |
| `agent-qa-control.py CONTROL OP [PAYLOAD]` | Controlar o driver de agente real de uma sessão Xvfb de teste | Opt-in; não controla janela pessoal. A sessão pode usar autenticação existente e consumir cota ao enviar prompts. Ver [agentes](../docs/checklists/agents-manual.md), sem expor credenciais. |
| `validate_skills.py` | Validar metadados das skills locais | Pode apontar divergências em skills de terceiros. Não é teste funcional do app e não justifica editar pacotes locais automaticamente. |

`tests/real-agent-driver.inc` pertence ao binário de testes; as imagens sintéticas estão em `tests/fixtures/agents/`. Nenhuma chamada a modelos deve ser iniciada pela auditoria, build ou suíte comum.

Artefatos de build e `.superpowers/` ficam ignorados. Eles podem conter caches ou evidências; não são lixo a apagar automaticamente. Backups antigos movidos estão em [docs/history/](../docs/history/README.md).

## Verificações de consolidação

- `bash tools/ci-check.sh development` / `default`: build, testes, traduções e instalação isolada; equivalente ao job nativo em `.github/workflows/ci.yml`.
- `python3 tools/benchmark.py --samples 3 --lines 3000`: matriz de 1/5/10 abas em Xvfb/Cairo, resultados em `build/benchmark.json`. `--binary` seleciona outro perfil; memória cobre somente o frontend, não os shells.
- `/usr/bin/python3 tests/check-accessible-atspi.py`: texto/caret/seleção/eventos através de AT-SPI em buses privados. Requisitos e limites em [desenvolvimento](../docs/development.md#consolidação-e-ci).

Os dois últimos testes são opt-in e não controlam janelas pessoais. O benchmark encerra seu próprio grupo de processos caso atinja o timeout.
