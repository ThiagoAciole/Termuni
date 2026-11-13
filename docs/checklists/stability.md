# Critérios para a primeira versão estável

Atualizado em 13/11/2026. **Termac é primeiro um terminal nativo GNOME completo; agentes são integrações opcionais.** O shell, PTY, entrada, desenho e restauração não podem depender de Claude Code/Codex instalados, autenticados ou disponíveis na rede.

Esta página é o contrato de aceite da versão estável. O [roadmap](../ROADMAP.md) contém melhorias; não é uma lista de bloqueadores integral. O [quadro vigente](../../PLANO_IMPLEMENTACAO.md) resume implementação e validação.

## Estados

- **Implementado:** código existe; a evidência indicada comprova apenas os cenários executados.
- **Em validação:** implementação existente, mas falta ambiente, cobertura ou aceite real.
- **Pendente:** falta código, correção ou teste específico.
- **Fora do escopo:** não exigido para a primeira versão estável.

## Ajustes arquiteturais: o que já foi feito

| Área | Estado e evidência | O que ainda falta |
| --- | --- | --- |
| Atalhos | **Implementado.** Ações em GAction/GSimpleAction; padrões editáveis em `editable_shortcuts`, persistência GSettings `action-shortcuts`, restauração e validação de conflitos. `/ui/shortcut-editor`, `/ui/terminal-shortcut-rebind` e `/ui/shortcut-conflict`. | Aceite de persistência no desktop/instalação; a lista de nomes reservados na janela ainda pode ser unificada em manutenção futura. Teclas encaminhadas ao shell pelo encoder não são ações de aplicação a eliminar. |
| Sessões | **Implementado e testado.** Formato v3 com geometria, títulos/pastas/seleção e conversão v2→v3 apenas sem v3. `/ui/session-v2-migration` agora prova preservação byte a byte do v2 e prevalência do v3 numa segunda ativação; `/ui/session-migration-write-failure` força erro real de gravação sem perder o v2. A origem não é apagada nem depois do sucesso. | Reinício entre processos/atualização (V07) e robustez da primeira gravação/durabilidade (M15). Reativação no mesmo processo não substitui todos os cenários de encerramento do aplicativo. |
| Agentes | **Implementado.** `termac-agent.c/.h` concentra registro, disponibilidade, formatação e comando. Claude Code e Codex são opcionais; `/ui/attach-without-agent-binary`, testes de fallback e boundary exercitam independência e isolamento. P2 registrou 6/6 imagens reais sob Xvfb. | Repetir recebimento multimodal pelo Flatpak/Wayland (V05). Outros agentes e eventos próprios ficam fora do escopo inicial. |
| Memória | **Investigação e correção já realizadas no P0.** Comparações de 3 e 15 ciclos identificaram uma referência de ícone por aba; foi corrigida. Depois, o relatório registrou o mesmo residual de bibliotecas: 22.689 bytes/128 blocos, sem frames do Termac. | Repetir a matriz com a versão candidata atual e explicar qualquer crescimento; não concluir saúde ou vazamento progressivo pelo total isolado. Os ~40 KB de uma observação anterior não são o diagnóstico atual. |

A auditoria desta rodada também corrigiu um erro crítico ao serializar **zero janelas**: um GVariant vazio tem zero bytes, mas `g_variant_store` exige destino não nulo. `/ui/save-empty-session` reproduziu a falha e passou após a correção. Uma sessão vazia continua sendo um estado v3 válido.

## TUI, Unicode, emoji e True Color

| Capacidade | Estado observado | Limites / próximo aceite |
| --- | --- | --- |
| vi e less | **Implementado/testado.** P0 cobre edição/salvamento pelo PTY, tela alternativa, resize e retorno ao shell. | Repetir os fluxos físicos no Wayland; M01 mantém abertos os cenários antigos de carga com falsos positivos. |
| TUI moderna | **btop 1.4.7 testado nesta rodada.** `/ui/modern-tui` usa configuração isolada CPU/memória, entra na tela alternativa, exibe dados, acompanha resize e sai com `q`, devolvendo o terminal. | É uma amostra concreta, não certificação de todos os recursos/TUIs. O teste pula explicitamente se btop não estiver instalado. Neovim, LazyGit e tmux não foram aceitos por inferência. |
| Unicode/emoji | **Implementado/testado no motor e desenho.** CJK, acento combinado, emoji ZWJ e seletor de variação preservados; `/vt/find-matches-wide` verifica células, e `/ui/truecolor-unicode-render` confere texto e produz captura inspecionada com emojis coloridos. | Casos ZWJ usam o modo de grafemas 2027; há teste da largura no modo legado. Fontes, IME CJK e escala fracionária reais continuam pendentes. Links após Unicode têm correção M02 ainda aberta. |
| True Color | **Implementado e verificado.** `/vt/truecolor` confirma foreground 17/83/149 e background 203/41/7 por SGR 24-bit, sem quantizar para paleta; o teste GTK verifica o pixel de fundo RGB 203/41/7 no desenho real. | A configuração btop ativa True Color. `COLORTERM=truecolor` ainda não é anunciado explicitamente no shell/bridge; autodetecção de outros programas é M14. Não confundir isso com ausência do suporte RGB. |

Capturas em `/tmp/termac-stability-shots/truecolor-unicode.png` e `modern-tui-btop.png`, regeneráveis com `TERMAC_UI_SCREENSHOTS` e os testes correspondentes. A captura do btop exclui o painel de comandos dos processos. Não foi reiniciada nenhuma sessão pessoal.

## Aceite da versão estável

Não marcar a linha inteira por causa de um único teste ou pela existência do código.

- [ ] **Shell, PTY, entrada, seleção e colagem sem falhas graves conhecidas:** reparar a evidência de carga M01, validar cenários críticos Wayland/IME e avaliar encerramento M03. Falhas de corrupção, perda de entrada ou crash bloqueiam o lançamento.
- [ ] **Restauração correta após reinício:** testes de arquivo/reativação/migração passam; executar V07 entre processos, incluindo atualização e erro de escrita. Não reexecutar comandos salvos automaticamente.
- [ ] **Flatpak e GNOME Builder validados:** CLI já tem evidência; completar V01 pela IDE e instalação/execução real sem depender de diretório de build.
- [ ] **Claude Code e Codex nos fluxos de anexo declarados:** nativo/Xvfb 6/6 registrado; completar V05 nos ambientes anunciados. A ausência ou falha de um CLI deve manter o terminal utilizável.
- [ ] **Sem crescimento contínuo de memória sem explicação:** repetir tamanhos de churn comparáveis e separar frames do Termac de caches de biblioteca. O número bruto de bytes definitivos ou o exit code do script diagnóstico, isoladamente, não conclui o aceite.
- [ ] **Acessibilidade básica e múltiplas escalas verificadas:** nomes/teclado/HiDPI já têm testes; conteúdo do terminal para Orca depende de M04; executar V03/V06 no desktop real.
- [ ] **Interface e documentação consistentes:** comparação Xvfb e organização documentadas; obter aprovação visual real, conferir pt-BR e manter o quadro/roadmap/checklists sem estados contraditórios.

## O que não bloqueia o lançamento por pertencer ao Marco 5

Realce da busca e conflitos de atalhos já foram entregues no P3. Não devem ser reimplementados nem virar trabalho fictício. Recursos adicionais do Marco 5 — perfis, workspaces nomeados, eventos específicos de agentes, regras extras de hyperlink e expansões semelhantes — não bloqueiam integralmente a primeira versão.

Isso não dispensa corrigir um crash, corrupção de sessão ou falha grave de entrada só porque está em um componente de produtividade. **Prioridade: confiabilidade do emulador → persistência → distribuição/aceite GNOME e acessibilidade → melhorias adicionais.**

## Pendência específica de gravação

`g_file_set_contents` usa `CONSISTENT | ONLY_EXISTING`; isso não equivale a uma garantia completa de durabilidade da primeira criação após queda de energia. M15 deve definir gravação consistente/durável e evitar I/O bloqueante na UI. O v2 já é mantido como origem de recuperação. Referências primárias: [GLib.file_set_contents](https://docs.gtk.org/glib/func.file_set_contents.html) e [GLib.file_set_contents_full](https://docs.gtk.org/glib/func.file_set_contents_full.html). Não foi simulado corte de energia nesta rodada.
