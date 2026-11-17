# Documentação do Termuni

## Estado e decisões

| Documento | Uso |
| --- | --- |
| [Plano vigente](PLANO_IMPLEMENTACAO.md) | Estado dos marcos, evidências e próximo incremento. |
| [Roadmap](ROADMAP.md) | Melhorias ainda necessárias, prioridade e critério de conclusão. |
| [Requisitos e arquitetura](architecture/PDR.Arquiteture.md) | Escopo do produto, componentes, persistência e limites técnicos. |
| [Design](design/DESIGN.md) | Regras visuais atuais e referências. |
| [Desenvolvimento](development.md) | Build, perfis, testes e uso seguro das ferramentas. |
| [Orientações para agentes](../AGENTS.md) | Regras de trabalho no repositório. |
| [Estrutura de pastas](estrutura.md) | Mapa das pastas, arquivos de entrada e destinos para novos documentos. |
| [Fontes técnicas](references/SOURCES.md) | Documentação primária consultada durante o desenvolvimento. |
| [Índice de skills](references/SKILLS_INDEX.md) | Inventário de skills do projeto. |

O plano vigente e o roadmap orientam a próxima tarefa. Relatórios provam o que foi observado numa execução datada; checklists indicam aceite pendente. Um registro antigo não reabre funcionalidades já entregues.

## Checklists de aceite

- [Critérios de estabilidade e auditoria das recomendações](checklists/stability.md).

- [GNOME Builder, pacote `.deb`, Orca e escala](checklists/gnome-manual.md).
- [Agentes reais e anexos](checklists/agents-manual.md): matriz observada e reprodução opt-in.
- [Revisão visual](checklists/visual-review.md): capturas inspecionadas, diferenças e limites.

## Relatórios e planos executados

- [Preparação para GitHub, 17/11/2026](reports/2026-11-17-github-preparation.md).

- [M04 — conteúdo acessível, 13/11/2026](reports/2026-11-13-accessible-text.md).

- [Revisão arquitetural/estabilidade, 13/11/2026](reports/2026-11-13-stability-review.md).

- [Organização e auditoria, 13/11/2026](reports/2026-11-13-project-organization.md).
- [Resultados consolidados P0–P3, 13/11/2026](reports/2026-11-13-p0-p3.md).
- [P0 — confiabilidade](superpowers/plans/2026-11-13-p0-reliability.md).
- [P1 — integração GNOME](superpowers/plans/2026-11-13-p1-gnome-integration.md).
- [P2 — agentes](superpowers/plans/2026-11-13-p2-agent-experience.md).
- [P3 — acabamento](superpowers/plans/2026-11-13-p3-polish.md).

## Referências e histórico

- [Fontes técnicas](references/SOURCES.md) e [índice de skills](references/SKILLS_INDEX.md).
- [Ferramentas do projeto](../tools/README.md).
- [Histórico e backups preservados](history/README.md).

Novas evidências datadas vão em `reports/`, procedimentos de aceite em `checklists/` e planos de implementação em `superpowers/plans/`. Atualize o roadmap em vez de criar outra lista concorrente de pendências.

- [Consolidação de estabilidade e medições](reports/2026-11-13-stable-consolidation.md) — registro histórico anterior à adoção do `.deb`.

- [Pacote instalável, atualização isolada, Orca e diagnóstico do controle nativo](reports/2026-11-13-desktop-acceptance.md).
