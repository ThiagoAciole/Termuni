# Plano de implementação do Termac

Atualizado em 13/11/2026. Este é o quadro vigente; a lista priorizada de melhorias está em [docs/ROADMAP.md](docs/ROADMAP.md). [docs/README.md](docs/README.md) reúne os demais documentos.

## Estado atual — 13/11/2026

**Base do terminal e MVP de agentes implementados; primeira versão estável ainda em validação.** P0–P3 entregaram código e evidências automatizadas. Uma funcionalidade existente com validação manual pendente não deve voltar à lista de “não implementado”.

| Marco | Situação | Evidência e trabalho restante |
| --- | --- | --- |
| 0 — dependência e build | Implementado; Builder pendente | C/Meson, Ghostty fixado e Zig 0.16.0. Builds development/default, schema instalado e Flatpak CLI registrados. Falta executar e observar pela interface do GNOME Builder. |
| 1 — shell e PTY | Implementado; robustez parcial | P0 cobre vi, resize, retorno ao shell, colagem de 2 MiB e encerramento sem órfãos; P1 cobre PTY do host no Flatpak. Faltam corrigir cenários antigos de carga e medir encerramento/desempenho (M01/M03/M06). |
| 2 — interação | Implementado; aceite Wayland parcial | Seleção/cópia/colagem, mouse SGR, teclas mortas e preedit têm cobertura. Faltam ibus/fcitx/CJK e Ctrl+Shift+U reais, escala fracionária e corrigir hit testing de links Unicode (M02). |
| 3 — interface | Implementado; aprovação real pendente | Uma/duas abas, overview, paletas, Preferências, Sobre, atalhos e busca inspecionados em Xvfb no P3. Fontes, bordas, ícones e aparência no compositor real permanecem na checklist. |
| 4 — imagens para agentes | Implementado (MVP); em validação no Flatpak | Claude Code e Codex: PNG/JPEG/Ctrl+V reconhecidos nos CLIs reais sob Xvfb, 6/6. Adaptadores e seleção explícita implementados. Repetir recebimento multimodal pelo Flatpak; outros agentes estão fora do MVP. |
| 5 — produtividade | Parcial; núcleo implementado | Busca com realce/navegação, editor com conflitos, links HTTP(S), restauração v3, atenção e política de notificações existem. Falta entrega/clique da notificação no GNOME; OSC 8 com rótulo, busy/idle host e eventos por agente são melhorias separadas; o Marco 5 inteiro não é bloqueador de lançamento. |
| 6 — consolidação | Parcial | Valgrind corrigiu referência de ícone por aba; suíte normal/HiDPI e nomes/papel acessíveis existem. Texto acessível (M04) implementado com offsets Unicode, caret, seleção e notificações; regressão em Xvfb passou. Faltam aceite Orca/AT-SPI, benchmark, CI e aceite desktop/distribuição. |

As referências atuais de produto e arquitetura são [PDR.Arquiteture.md](PDR.Arquiteture.md) e [DESIGN.md](DESIGN.md). Os recuos atuais são **12 unidades acima / 8 à esquerda**. Perfis de terminal, outros agentes e gráficos inline não são requisitos da primeira versão estável.

## Próximo incremento recomendado

1. **M01 — evidência confiável do emulador:** isolar os cenários de interface e provar que os comandos de carga realmente executaram.
2. **M03/M15 — encerramento e persistência:** eliminar bloqueio ao parar shells e definir durabilidade da primeira gravação; a sessão vazia foi corrigida e a migração v2→v3 ganhou regressões de idempotência/falha.
3. **V01/V05/V07 e M04 — aceite da distribuição, reinício e acessibilidade:** Builder/Flatpak reais, restauração entre processos e leitura do conteúdo no Orca.

M02 (links Unicode) continua uma correção concreta aberta. Melhorias adicionais do Marco 5 não devem preceder confiabilidade, persistência e distribuição. Os [critérios de estabilidade](docs/checklists/stability.md) distinguem **implementado**, **em validação**, **pendente** e **fora do escopo**; a checklist completa, não o número de marcos implementados, define o aceite.

TUI/Unicode/True Color: vi/less têm cobertura anterior; nesta revisão btop 1.4.7, preservação de CJK/emoji e RGB exato foram exercitados em Xvfb. A matriz e seus limites ficam na mesma checklist. Isso não certifica todas as TUIs nem escala/IME do compositor real.

## Prioridades — 13/11/2026

Os planos originais continuam como contratos dos incrementos executados, não como backlog atual.

| Incremento | Código/evidência entregue | Aceite adicional | Plano |
| --- | --- | --- | --- |
| P0 — confiabilidade | PTY/vi, entrada, mouse, paste, isolamento, restauração e Valgrind | Revisar cenários antigos e validar IME/Wayland | [P0](docs/superpowers/plans/2026-11-13-p0-reliability.md) |
| P1 — integração GNOME | Schema/instalação, Flatpak/host, ícones, nomes acessíveis e HiDPI | Builder, Orca, ícones/escala no desktop, imagem real Flatpak | [P1](docs/superpowers/plans/2026-11-13-p1-gnome-integration.md) |
| P2 — agentes | Seleção/adaptadores e 6/6 anexos reais em Claude Code/Codex | Gestos Wayland e repetição pelo Flatpak | [P2](docs/superpowers/plans/2026-11-13-p2-agent-experience.md) |
| P3 — acabamento | Realce, conflitos, política de notificações e comparação visual | Aprovação visual e notificação GNOME | [P3](docs/superpowers/plans/2026-11-13-p3-polish.md) |

## Evidências e limites

- [Revisão arquitetural e critérios de estabilidade](docs/reports/2026-11-13-stability-review.md): recomendações confrontadas com código, sessão vazia corrigida, migração/falha de gravação e TUI/Unicode/True Color verificados. Suíte atual **11/11**, UI normal 173,88 s e HiDPI 188,51 s (49 casos UI, quatro auxiliares opt-in pulados); default rápido 3/3 e build passaram.
- [Relatório consolidado P0–P3](docs/reports/2026-11-13-p0-p3.md): execuções até `5a14a91`. Suíte completa histórica do P3: **11/11**, UI normal 164,74 s e HiDPI 177,67 s; 45 casos UI, quatro auxiliares opt-in pulados. Builds development/default e 157 traduções, zero fuzzy.
- [Revisão visual](docs/checklists/visual-review.md): 32 capturas abertas, 19 principais; limitações dos cenários auxiliares herdados estão explícitas. Suíte verde não comprova que esses cenários de carga executaram o comando pretendido.
- [Aceite GNOME/Builder](docs/checklists/gnome-manual.md) e [agentes](docs/checklists/agents-manual.md): não interpretar Xvfb ou build CLI como comprovação de interação no compositor/IDE.
- [Auditoria desta organização](docs/reports/2026-11-13-project-organization.md): build e seis suítes rápidas passaram; links, histórico/backups e inventário conferidos. [Roadmap](docs/ROADMAP.md) baseado no código até `5a14a91`, incluindo reprodução isolada do link CJK. Nenhuma sessão pessoal foi reiniciada.

## Organização e manutenção

**Diretriz de produto:** primeiro um excelente terminal nativo GNOME; agentes opcionais como diferencial. A indisponibilidade de Claude Code/Codex não pode impedir shell, PTY, entrada ou restauração.

- Estado e próximos passos: este arquivo e [roadmap](docs/ROADMAP.md).
- Requisitos/decisões de produto: [PDR.Arquiteture.md](PDR.Arquiteture.md); aparência: [DESIGN.md](DESIGN.md).
- Comandos de desenvolvimento e critérios de teste: [docs/development.md](docs/development.md); ferramentas: [tools/README.md](tools/README.md).
- Resultados datados: `docs/reports/`; planos executáveis: `docs/superpowers/plans/`; aceite manual: `docs/checklists/`.
- [Histórico anterior à organização](docs/history/implementation-before-organization-2026-11-13.md): preserva as 597 linhas anteriores, inclusive decisões e pendências já superadas. Não é o quadro vigente.
- [Backups antigos](docs/history/README.md): arquivos preservados e retirados dos diretórios ativos; builds/caches e novos backups de editor ficam ignorados pelo Git.

Ao concluir um incremento, atualize o estado aqui, o item correspondente no roadmap e a evidência datada. Evite acrescentar diários longos a este quadro ou copiar listas antigas de pendências.
