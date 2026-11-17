# Critérios para a primeira versão estável

Atualizado em 13/11/2026. **Termuni é primeiro um terminal nativo GNOME completo; agentes são integrações opcionais.** O shell, PTY, entrada, desenho e restauração não podem depender de Claude Code/Codex instalados, autenticados ou disponíveis na rede.

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
| Sessões | **Implementado e testado.** Formato v3 com geometria, títulos/pastas/seleção e conversão v2→v3 apenas sem v3. `/ui/session-v2-migration` agora prova preservação byte a byte do v2 e prevalência do v3 numa segunda ativação; `/ui/session-migration-write-failure` força erro real de gravação sem perder o v2. A origem não é apagada nem depois do sucesso. | Reinício entre processos e migração/erro/retry passaram nativamente. M15 usa writer ordenado, CONSISTENT e DURABLE e modo 0600; testes de criação/substituição/erro passaram. Conferir o fluxo de atualização do pacote distribuído no aceite desktop. |
| Agentes | **Implementado.** `termuni-agent.c/.h` concentra registro, disponibilidade, formatação e comando. Claude Code e Codex são opcionais; `/ui/attach-without-agent-binary`, testes de fallback e boundary exercitam independência e isolamento. P2 registrou 6/6 imagens reais sob Xvfb. | Os CLIs são locais; a repetição de imagens reais marcou 6/6. Gestos físicos Wayland permanecem no aceite desktop. Outros agentes e eventos próprios ficam fora do escopo inicial. |
| Memória | **Investigação e correção já realizadas no P0.** Comparações de 3 e 15 ciclos identificaram uma referência de ícone por aba; foi corrigida. Depois, o relatório registrou o mesmo residual de bibliotecas: 22.689 bytes/128 blocos, sem frames do Termuni. | Matriz integrada de 3/15 ciclos repetida: 22.016 bytes/20 blocos em ambas, sem frames do Termuni nas perdas definitivas nem acessos inválidos reportados. Explicar crescimento futuro por comparação, sem julgar pelo total isolado. Os ~40 KB de uma observação anterior não são o diagnóstico atual. |

A auditoria desta rodada também corrigiu um erro crítico ao serializar **zero janelas**: um GVariant vazio tem zero bytes, mas `g_variant_store` exige destino não nulo. `/ui/save-empty-session` reproduziu a falha e passou após a correção. Uma sessão vazia continua sendo um estado v3 válido.

## TUI, Unicode, emoji e True Color

| Capacidade | Estado observado | Limites / próximo aceite |
| --- | --- | --- |
| vi e less | **Implementado/testado.** P0 cobre edição/salvamento pelo PTY, tela alternativa, resize e retorno ao shell. | Repetir os fluxos físicos no Wayland; M01 isolou carga, entrada, seleção, anexos e TUIs, com marcadores de execução e conferência integral da saída. |
| TUI moderna | **btop 1.4.7 testado nesta rodada.** `/ui/modern-tui` usa configuração isolada CPU/memória, entra na tela alternativa, exibe dados, acompanha resize e sai com `q`, devolvendo o terminal. | É uma amostra concreta, não certificação de todos os recursos/TUIs. O teste pula explicitamente se btop não estiver instalado. Neovim, LazyGit e tmux não foram aceitos por inferência. |
| Unicode/emoji | **Implementado/testado no motor e desenho.** CJK, acento combinado, emoji ZWJ e seletor de variação preservados; `/vt/find-matches-wide` verifica células, e `/ui/truecolor-unicode-render` confere texto e produz captura inspecionada com emojis coloridos. | Casos ZWJ usam o modo de grafemas 2027; há teste da largura no modo legado. Fontes, IME CJK e escala fracionária reais continuam pendentes. M02/M09 corrigiram links Unicode e OSC 8 usando células/grafemas reais, com regressões. |
| True Color | **Implementado e verificado.** `/vt/truecolor` confirma foreground 17/83/149 e background 203/41/7 por SGR 24-bit, sem quantizar para paleta; o teste GTK verifica o pixel de fundo RGB 203/41/7 no desenho real. | A configuração btop ativa True Color. `COLORTERM=truecolor` é exportado no shell local e os testes conferem esse valor. Não confundir isso com ausência do suporte RGB. |

Capturas em `/tmp/termuni-stability-shots/truecolor-unicode.png` e `modern-tui-btop.png`, regeneráveis com `TERMUNI_UI_SCREENSHOTS` e os testes correspondentes. A captura do btop exclui o painel de comandos dos processos. Não foi reiniciada nenhuma sessão pessoal.

## Aceite da versão estável

Não marcar a linha inteira por causa de um único teste ou pela existência do código.

- [ ] **Shell, PTY, entrada, seleção e colagem sem falhas graves conhecidas:** M01 e M03 passaram; falta validar cenários físicos críticos Wayland/IME. Falhas de corrupção, perda de entrada ou crash bloqueiam o lançamento.
- [x] **Restauração correta após reinício (nativo):** V07 usa executáveis em processos distintos, incluindo migração v2, prevalência de v3 e erro/retry de gravação. A instalação/atualização do pacote permanece no aceite da distribuição. Não reexecutar comandos salvos automaticamente.
- [ ] **GNOME Builder e `.deb` validados:** perfil default instalado via pacote local e schema/ícone/ID conferidos; completar V01 pela IDE e o aceite visual/Wayland no desktop.
- [x] **Claude Code e Codex nos fluxos funcionais de anexo declarados:** 6/6 nos CLIs nativos, com conteúdo visual reconhecido. Gestos físicos no compositor permanecem separados. A ausência ou falha de um CLI deve manter o terminal utilizável.
- [x] **Sem crescimento contínuo observado nos ciclos medidos:** churn de 3/15 ciclos com perdas definitivas constantes e sem frames do Termuni; suíte também compara RSS após aquecimento/churn. Esse resultado cobre as cargas medidas, sem prometer memória constante em qualquer aplicação. O número bruto de bytes definitivos ou o exit code do script diagnóstico, isoladamente, não conclui o aceite.
- [ ] **Acessibilidade básica e múltiplas escalas verificadas:** nomes/teclado/HiDPI já têm testes; texto/caret/seleção/eventos passaram por AT-SPI real; executar V03/V06 no desktop real.
- [ ] **Interface e documentação consistentes:** comparação Xvfb e organização documentadas; obter aprovação visual real, conferir pt-BR e manter o quadro/roadmap/checklists sem estados contraditórios.

## O que não bloqueia o lançamento por pertencer ao Marco 5

Realce da busca e conflitos de atalhos já foram entregues no P3. Não devem ser reimplementados nem virar trabalho fictício. Recursos adicionais do Marco 5 — perfis, workspaces nomeados, eventos específicos de agentes, regras extras de hyperlink e expansões semelhantes — não bloqueiam integralmente a primeira versão.

Isso não dispensa corrigir um crash, corrupção de sessão ou falha grave de entrada só porque está em um componente de produtividade. **Prioridade: confiabilidade do emulador → persistência → distribuição/aceite GNOME e acessibilidade → melhorias adicionais.**

## Política de gravação entregue

M15 substituiu a escrita comum por snapshots imutáveis e um writer dedicado, com coalescência/ordenação, drenagem no shutdown e checkpoint explícito no quit. A primeira criação e substituição usam `CONSISTENT | DURABLE`, modo 0600; falha mantém o arquivo anterior e o v2. A migração não sobrepõe um v3 existente com dados antigos. Testes conferem flags, permissões, ordem, erro e retry; não foi simulado corte físico de energia. Referência primária: [GLib.file_set_contents_full](https://docs.gtk.org/glib/func.file_set_contents_full.html).
