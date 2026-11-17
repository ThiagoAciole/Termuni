# Plano de implementação do Termuni

Atualizado em 17/11/2026. Este é o quadro vigente; a lista priorizada de melhorias está no [roadmap](ROADMAP.md). O [índice da documentação](README.md) reúne os demais documentos.

## Estado atual — 17/11/2026

**Base do terminal e MVP de agentes implementados; primeira versão estável ainda em validação.** P0–P3 entregaram código e evidências automatizadas. Uma funcionalidade existente com validação manual pendente não deve voltar à lista de “não implementado”.

| Marco | Situação | Evidência e trabalho restante |
| --- | --- | --- |
| 0 — dependência e build | Implementado; Builder pendente | C/Meson, Ghostty fixado e Zig 0.16.0. Builds development/default e pacote `.deb` local verificados; instalação 0.1.0 amd64 conferida. Falta executar e observar pela interface do GNOME Builder. |
| 1 — shell e PTY | Implementado; robustez parcial | P0 cobre vi, resize, retorno ao shell, colagem de 2 MiB e encerramento sem órfãos; a distribuição nativa usa shell/PTY local. Carga/interrupção isoladas e encerramento assíncrono com regressões passaram; baseline de 1/5/10 abas registrado; aceite físico Wayland pendente. |
| 2 — interação | Implementado; aceite Wayland parcial | Seleção/cópia/colagem, mouse SGR, teclas mortas e preedit têm cobertura. Faltam ibus/fcitx/CJK e Ctrl+Shift+U reais, escala fracionária; links Unicode e OSC 8 foram corrigidos a partir das células do motor. |
| 3 — interface | Implementado; aprovação real pendente | Uma/duas abas, overview, paletas, Preferências, Sobre, atalhos e busca inspecionados em Xvfb no P3. Fontes, bordas, ícones e aparência no compositor real permanecem na checklist. |
| 4 — imagens para agentes | Implementado (MVP); validação funcional nativa | Claude Code e Codex: PNG/JPEG/Ctrl+V reconhecidos nos CLIs reais sob Xvfb, 6/6. Adaptadores e seleção explícita implementados. Os CLIs executam como processos locais; recebimento visual já passou 6/6 no ambiente de QA. Gestos físicos Wayland pendentes; outros agentes fora do MVP. Em 17/11/2026: botão de início rápido (ícone de brilho, à esquerda de Nova aba) lista os agentes registrados e abre o escolhido pela ação `win.launch-agent-id`, com a mesma verificação de disponibilidade: com uma única aba, digita o comando no shell dela (se houver programa em execução, mostra aviso e não envia a tecla); com mais de uma, abre nova aba. Regressão `/ui/launch-agent-quick-menu` e suíte completa passaram, ícone conferido em captura Xvfb. Falta clique real no Wayland. |
| 5 — produtividade | Parcial; núcleo implementado | Busca com realce/navegação, editor com conflitos, links HTTP(S), restauração v3, atenção e política de notificações existem. Falta entrega/clique da notificação no GNOME; OSC 8 com rótulo implementado; busy/idle host e eventos por agente são melhorias separadas; o Marco 5 inteiro não é bloqueador de lançamento. |
| 6 — consolidação | Parcial | Valgrind corrigiu referência de ícone por aba; suíte normal/HiDPI e nomes/papel acessíveis existem. Texto acessível (M04) implementado com offsets Unicode, caret, seleção e notificações; regressão em Xvfb passou. Transporte AT-SPI real verificado em buses privados; CI e ferramenta de benchmark implementados. Orca 51 leu e gerou texto em teste privado; pacote default instalado/atualizado isoladamente com preferência preservada. Faltam áudio/navegação Orca e aceite físico do desktop/Wayland. |

O formato escolhido para distribuir e instalar o Termuni é `.deb` nativo. Em 17/11/2026 o app foi renomeado para Termuni com ID `io.github.ThiagoAciole.Termuni` (`.Devel` no perfil de desenvolvimento). Configurações e dados foram copiados com `tools/migrate-termac-to-termuni.sh`, que não apaga os originais; o pacote `thiago.aciole.termac` foi removido. Suíte completa passou no build renomeado, o `.deb` instalado e o binário conferido; falta aceite visual do novo pacote. Registros de builds anteriores ficam apenas no histórico.

As referências atuais de produto e arquitetura são [PDR.Arquiteture.md](architecture/PDR.Arquiteture.md) e [DESIGN.md](design/DESIGN.md). Os recuos atuais são **12 unidades acima / 8 à esquerda**. Perfis de terminal, outros agentes e gráficos inline não são requisitos da primeira versão estável.

## Consolidação e próximos aceites

- [Migração para distribuição `.deb`](reports/2026-11-14-local-deb-install.md): empacotador reproduzível criado; código com shell/PTY locais compilado, empacotado e reinstalado como `io.github.ThiagoAciole.Termuni` 0.1.0/amd64. Launcher, schema, serviço D-Bus e ícone `io.github.ThiagoAciole.Termuni` conferidos nos caminhos `/usr`; o app antigo de desenvolvimento foi removido, preservando seus dados. Ainda falta aceite visual.

M01/M02/M03/M09/M10/M12/M14/M15 têm implementação e regressões. Sessões agora usam writer ordenado em background com gravação consistente/durável; saída confirma programas ativos e aguarda coleta dos filhos sem bloquear a UI. O teste de reinício usa executáveis em processos distintos e preserva a origem v2.

Acessibilidade expõe texto/caret/seleção Unicode e seus eventos foram lidos por AT-SPI em D-Bus privado. CI cobre ambos os perfis e possui equivalente local; a execução remota ainda depende de publicar o repositório. O benchmark opt-in mediu 1/5/10 abas em 9 amostras, sem envolver sessões pessoais.

Restam principalmente os aceites **V01–V04/V06**: interação pela IDE Builder, Wayland/IME, aparência/escala fracionária, notificações GNOME, uso do Orca e instalação/atualização no desktop. Os [critérios de estabilidade](checklists/stability.md) distinguem implementação de aceite; não declarar a versão estável apenas porque o build passou.

TUI/Unicode/True Color: anúncio COLORTERM no shell/bridge e regressões de células Unicode entregues; vi/less têm cobertura anterior; nesta revisão btop 1.4.7, preservação de CJK/emoji e RGB exato foram exercitados em Xvfb. A matriz e seus limites ficam na mesma checklist. Isso não certifica todas as TUIs nem escala/IME do compositor real.

## Preparação para publicar no GitHub — 17/11/2026

README refeito como página inicial para visitantes, com resumo do produto, estado de desenvolvimento, requisitos, comandos de build/teste, mapa do repositório, documentação e licença. Nesta reorganização, plano, arquitetura, design, fontes e índice de skills foram agrupados em `docs/`. Os builds, caches, pacotes e exportações antigos foram preservados sob `build/archive/`, `build/cache/flatpak-builder` e `build/packages/archive`; os novos builds têm diretórios distintos dentro de `build/`. O workflow `/.github/workflows/ci.yml` continua cobrindo os perfis `development` e `default`.

O [mapa da estrutura](estrutura.md) explica os diretórios ativos, os arquivos mantidos na raiz e onde adicionar documentos e builds. Os caminhos em scripts, comandos, guias e links foram atualizados. As configurações Meson arquivadas apontavam para `/home/thiago/Projects/termac`; foram preservadas como arquivo, mas não são builds ativos. Os novos diretórios Meson serão criados sob demanda.

Verificação: diretórios e pacotes foram movidos sem apagar seu conteúdo; links Markdown locais foram conferidos; `git diff --check` passou. Não rodei compilação nem testes nesta reorganização. O repositório ainda não tem remoto GitHub configurado nesta cópia, e os aceites GNOME/Wayland deste plano continuam pendentes.

## Prioridades — 13/11/2026

Os planos originais continuam como contratos dos incrementos executados, não como backlog atual.

| Incremento | Código/evidência entregue | Aceite adicional | Plano |
| --- | --- | --- | --- |
| P0 — confiabilidade | PTY/vi, entrada, mouse, paste, isolamento, restauração e Valgrind | Revisar cenários antigos e validar IME/Wayland | [P0](superpowers/plans/2026-11-13-p0-reliability.md) |
| P1 — integração GNOME | Schema/instalação `.deb`, shell/PTY local, ícones, nomes acessíveis e HiDPI | Builder, Orca, ícones e escalas no desktop; aceite visual continua pendente | [P1](superpowers/plans/2026-11-13-p1-gnome-integration.md) |
| P2 — agentes | Seleção/adaptadores e 6/6 anexos reais em Claude Code/Codex | Gestos Wayland e anexos pelo compositor real | [P2](superpowers/plans/2026-11-13-p2-agent-experience.md) |
| P3 — acabamento | Realce, conflitos, política de notificações e comparação visual | Aprovação visual e notificação GNOME | [P3](superpowers/plans/2026-11-13-p3-polish.md) |

## Evidências e limites

- [Janelas auxiliares sem sobreposição](reports/2026-11-13-dialog-windows.md): confirmações, renomear, Sobre, Atalhos e captura de atalhos agora usam janelas nativas separadas. Cancelar/X preservam a sessão; fechar a origem encerra as auxiliares. Regra visual registrada no DESIGN.md.
- [Aceite adicional: pacote e Orca](reports/2026-11-13-desktop-acceptance.md): bundle default instalado/atualizado em instalação isolada, fonte 17 preservada; Orca 51 leu/gerou o texto Unicode esperado. Runner AT-SPI força Xvfb privado mesmo com DISPLAY herdado. Controle da IDE/Wayland não ficou operacional; CI aguarda destino Git.
- [Consolidação de estabilidade](reports/2026-11-13-stable-consolidation.md): **13/13** nos dois perfis, 58 casos UI (seis auxiliares opt-in), AT-SPI real, sessão entre processos, PTY/coleta nativa, **6/6 imagens reais nos CLIs**, Valgrind 3/15 e nove amostras de benchmark. Código/aceite físico permanecem separados.
- [Revisão arquitetural e critérios de estabilidade](reports/2026-11-13-stability-review.md): recomendações confrontadas com código, sessão vazia corrigida, migração/falha de gravação e TUI/Unicode/True Color verificados. Suíte da revisão anterior **11/11**, UI normal 173,88 s e HiDPI 188,51 s (49 casos UI, quatro auxiliares opt-in pulados); default rápido 3/3 e build passaram.
- [Relatório consolidado P0–P3](reports/2026-11-13-p0-p3.md): execuções até `5a14a91`. Suíte completa histórica do P3: **11/11**, UI normal 164,74 s e HiDPI 177,67 s; 45 casos UI, quatro auxiliares opt-in pulados. Builds development/default e 157 traduções, zero fuzzy.
- [Revisão visual](checklists/visual-review.md): 32 capturas abertas, 19 principais; limitações dos cenários auxiliares herdados estão explícitas. Suíte verde não comprova que esses cenários de carga executaram o comando pretendido.
- [Aceite GNOME/Builder](checklists/gnome-manual.md) e [agentes](checklists/agents-manual.md): não interpretar Xvfb ou build CLI como comprovação de interação no compositor/IDE.
- [Auditoria desta organização](reports/2026-11-13-project-organization.md): build e seis suítes rápidas passaram; links, histórico/backups e inventário conferidos. [Roadmap](ROADMAP.md) baseado no código até `5a14a91`, incluindo reprodução isolada do link CJK. Nenhuma sessão pessoal foi reiniciada.

## Organização e manutenção

**Diretriz de produto:** primeiro um excelente terminal nativo GNOME; agentes opcionais como diferencial. A indisponibilidade de Claude Code/Codex não pode impedir shell, PTY, entrada ou restauração.

- Estado e próximos passos: este arquivo e [roadmap](ROADMAP.md).
- Requisitos/decisões de produto: [PDR.Arquiteture.md](architecture/PDR.Arquiteture.md); aparência: [DESIGN.md](design/DESIGN.md).
- Comandos de desenvolvimento e critérios de teste: [docs/development.md](development.md); ferramentas: [tools/README.md](../tools/README.md).
- Resultados datados: `docs/reports/`; planos executáveis: `docs/superpowers/plans/`; aceite manual: `docs/checklists/`.
- [Histórico anterior à organização](history/implementation-before-organization-2026-11-13.md): preserva as 597 linhas anteriores, inclusive decisões e pendências já superadas. Não é o quadro vigente.
- [Backups antigos](history/README.md): arquivos preservados e retirados dos diretórios ativos; builds/caches e novos backups de editor ficam ignorados pelo Git.

Ao concluir um incremento, atualize o estado aqui, o item correspondente no roadmap e a evidência datada. Evite acrescentar diários longos a este quadro ou copiar listas antigas de pendências.
