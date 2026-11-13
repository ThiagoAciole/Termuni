# Organização e auditoria — 13/11/2026

Branch `chore/project-organization`, a partir de `5a14a91` (P3). Escopo: organização da documentação/artefatos e identificação de melhorias; o código do aplicativo, IDs, permissões e sessões pessoais foram preservados.

## Entregas

- O quadro vigente foi separado do diário antigo. As 597 linhas anteriores estão no histórico; a conferência automatizada comprovou preservação integral, exceto pelo aviso inicial e ajuste dos links relativos.
- Evidências P0–P3 foram extraídas para relatório datado; requisitos/arquitetura e orientações de entrada apontam para o estado atual.
- Criados índice de documentação, guia de desenvolvimento, catálogo de ferramentas e roadmap M01–M13, separado do aceite V01–V06.
- Backups `po/pt_BR.po~` e `thiago.aciole.termac.json~` foram movidos para `docs/history/backups/`. SHA-256 antes/depois idênticos; nenhum byte alterado.
- Regras de Git para builds/caches/backup de editor passaram a ser versionadas. Nenhum build/cache foi apagado.
- O inventário agora ignora artefatos e Ghostty baixado, mantendo a integração Meson local. Antes varria 1.213 arquivos e detectava o frontend GTK/internal do upstream como sinal; depois varreu 41 arquivos do projeto sem esses falsos positivos. Isso continua sendo heurística, não prova de ABI/linkagem.

## Achados técnicos

O [roadmap](../ROADMAP.md) contém evidências e critérios. Prioridades: testes UI com estado compartilhado, hit testing Unicode dos links, encerramento síncrono e texto acessível.

Reprodução isolada do helper de URL, compilado com o `termac-util.c` atual: entrada `日本語 https://example.org`, coluna 4, resultou em `hit=1` e `first_column=4`; o início visual em células é 7. Os chamadores do terminal passam coordenadas de células. Não foi aberto navegador nem exercitado clique no desktop; o teste de integração faz parte de M02.

O bloqueio de encerramento e a ausência de `GtkAccessibleText` foram identificados no código. Não foram implementados nem se declarou medição de latência/aceite Orca nesta rodada.

## Verificações desta rodada

| Verificação | Resultado |
| --- | --- |
| `meson compile -C build` | Passou; nenhum rebuild do app necessário. |
| `meson test -C build util vt attachments settings agent agent-names-contained --print-errorlogs` | 6/6 passaram. A suíte gráfica completa não foi repetida porque não houve alteração de runtime/UI. |
| `inspect_termac.py --root . --json` | libghostty-vt identificado pelo frontend; zero erros de leitura. |
| Fixture isolada da auditoria | Preserva fonte e integração Meson; ignora upstream, build-default, cache Flatpak e scratch de agentes. |
| Links locais e histórico | Links Markdown conferidos; histórico completo e backups preservados. |
| `python3 -m py_compile tools/inspect_termac.py` / `git diff --check` | Passaram. |

Ambiente consultado: GTK 4.24.1, libadwaita 1.10.0, Zig 0.16.0, Meson 1.10.1. O registro anterior de suíte completa P3 permanece 11/11; não é uma nova execução de todos os cenários nem comprovação de desktop real.
