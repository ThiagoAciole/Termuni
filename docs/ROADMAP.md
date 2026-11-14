# Melhorias do Termac

Auditoria de 13/11/2026 sobre o código até `5a14a91`, após P0–P3. **Este é o backlog vigente**, complementado pela revisão arquitetural/estabilidade sobre `51fa0b4`. A [consolidação posterior](reports/2026-11-13-stable-consolidation.md) atualiza os estados de implementação e evidencia os aceites restantes. Os [critérios de estabilidade](checklists/stability.md) definem bloqueadores de lançamento; o backlog inteiro não define. O [plano atual](../PLANO_IMPLEMENTACAO.md) resume o estado; [evidências anteriores](reports/2026-11-13-p0-p3.md) não são substituídas por esta organização.

Tipos: **correção** = comportamento inadequado identificado; **implementação** = capacidade ausente; **validação** = código existe, falta comprovar o aceite. “Alta” indica a ordem recomendada de trabalho, não uma declaração de que todas as versões já falham nesses cenários.

## Ordem recomendada

| ID | Prioridade / tipo | Melhoria e evidência atual | Critério para concluir |
| --- | --- | --- | --- |
| M01 | Alta / correção de QA | **Implementado.** Carga/interrupção, seleção/cópia, anexos, less/vi, mouse, IME e paste usam shells próprios. Marcadores de execução e arquivo de 30.000 linhas eliminam os falsos positivos. Ver relatório de consolidação. | Limpar ou criar sessão por cenário; usar marcadores de saída que não possam ser satisfeitos pelo comando apenas ecoado; comprovar execução de `seq 1 30000`, retorno ao prompt e ausência de perda de entrada; selecionar Anexos antes da captura; passar cada caso sozinho e em suíte normal/HiDPI. |
| M02 | Alta / correção de produto | **Implementado.** Links usam células/grafemas do motor; regressões CJK, combining, ZWJ e modo legado passaram. Hover/abertura compartilham o mesmo resolvedor. | Regressões CJK, acento combinado, emoji/ZWJ e seleção; usar geometria real do motor, não substituir por outra estimativa de largura; Ctrl+clique e hover coincidirem com o texto. |
| M03 | Alta / robustez | **Implementado; bridge verificado.** Child watch assume o PID sem reter o widget; SIGHUP e escalonamento para SIGKILL são assíncronos. Testes nativos medem retorno abaixo de 250 ms, liberação do widget e coleta; saída do processo também cobre filho resistente. | Encerramento assíncrono com ownership explícito de PID/watch/fd; janela responde enquanto o filho ignora SIGHUP; prazo para SIGKILL e coleta sem órfãos/zumbis, inclusive no bridge Flatpak; fechar várias abas não acumula esperas na UI. |
| M04 | Alta / aceite manual | **Implementado e transporte AT-SPI verificado.** Texto Unicode, caret, seleção e eventos chegaram ao cliente AT-SPI por buses privados. Orca 51 leu/gerou texto em teste privado; falta áudio/navegação com Orca no Wayland. | Regressão automatizada passou em Xvfb; validar transporte AT-SPI e saída/digitação/seleção no Orca/Wayland. Setters acessíveis retornam `FALSE`, sem alterar o cursor do aplicativo no PTY. |
| M05 | Média / infraestrutura | **Implementado; execução remota pendente.** Workflow nativo/Flatpak para development/default, script local, instalação isolada, traduções, logs e teste AT-SPI privado. Nenhuma conta de agente é exigida. | Build com Zig/revisão fixados, testes unitários/UI/HiDPI, schema/Desktop/AppStream, traduções e ambos os perfis; publicar logs/artefatos em falha. Contas reais de agentes e interação do desktop continuam opt-in; não exigir credenciais na CI comum. |
| M06 | Média / medição e desempenho | **Implementado; baseline Xvfb medido.** Benchmark opt-in de 1/5/10 abas: 9 amostras, 3.000 linhas exatas por aba, JSON versionado, RSS/CPU, busca e resize. Cairo/Xvfb identificado; perfil GPU/Wayland permanece separado. | Após M01, registrar ambiente/build/GPU, amostras comparáveis e picos, foco/segundo plano, busca e resize. Otimizar caches/regiões de desenho ou trocar renderer somente depois de localizar o custo e medir novamente. |
| M07 | Média / implementação | **Busy/idle do shell host no Flatpak.** `terminal_has_job` considera toda sessão host viva ocupada porque não enxerga seus grupos de processos. A confirmação conservadora é intencional e protege trabalho. | Protocolo host confiável de estado/cancelamento, com regressões para prompt, tarefa longa, saída, agente e perda do bridge; preservar a proteção quando o estado for desconhecido. Não ampliar permissões como atalho. |
| M08 | Média / implementação e validação | **Integração de shell além de Bash.** O Bash host informa CWD por OSC 7; outros shells dependem de configuração própria. Conclusão de comandos está disponível na integração Bash opt-in, sem eventos específicos dos agentes. | Escolher um shell por incremento (ex.: Zsh ou Fish), preservar inicialização/dotfiles, validar CWD Unicode/restauração e códigos de saída. Suporte anunciado deve ter cenário real; não inferir estado pelo texto do prompt. |
| M09 | Média / implementação | **Implementado.** URI OSC 8 e seus limites vêm das células do motor; apenas HTTP(S) válido é aberto por ação explícita. Regressões para rótulos, células largas e protocolos inválidos passaram. | Rótulo não URL clicável a partir dos dados do motor, URI/limites validados, ação explícita do usuário, hover/cancelamento/foco corretos e regressões de entrada malformada. Preservar links textuais após M02. |
| M10 | Média / UX e robustez | **Implementado.** Fila cheia e conexão encerrada emitem feedback traduzido, limitado por episódio; erro de escrita limpa entrada não entregável. Teste de fechamento e colagem de 2 MiB com receptor real passaram. | Política definida para excesso/erro de escrita, feedback traduzido e teste de receptor lento/falha/fechamento; manter ordem e integridade abaixo do limite sem bloquear a UI. |
| M11 | Média / distribuição | **Preparar release verificável.** O manifesto ativo é development; o perfil default compila, mas não há homepage/releases publicados registrados nem aceite desktop completo. | Rodar V01–V06 abaixo, instalar pacote default isolado com ícone/schema próprios, testar atualização/persistência e registrar a versão distribuída. Adicionar URLs/releases apenas quando existirem; não inventar metadata. |
| M12 | Média / correção de ferramenta | **Implementado/testado.** Restart identifica caminho exato/perfil e usa lock; outras raízes/default são preservados. Teste cria processos fictícios próprios, incluindo inode substituído, falha e resistência ao encerramento. | Identificar PID/executável desta raiz e perfil, preservar outras instâncias e confirmar o escopo antes de encerrar; verificar com duas instâncias/projetos isolados. Não testar o restart contra sessões pessoais. |
| M13 | Posterior / manutenção | **Separação incremental de responsabilidades.** `termac-terminal.c` reúne PTY/entrada/renderização; `termac-application.c` reúne sessão/Preferências/atalhos; `test-ui.c` compartilha contexto e cresce. | Primeiro M01; depois extrair uma responsabilidade por vez, com interfaces pequenas, ownership/cancelamento e regressões de lifecycle. Preservar Meson/Builder, caminho de recursos e semântica de sessão. Nenhuma migração de toolkit/motor é necessária para organizar o projeto. |
| M14 | Média / implementação | **Implementado.** Shell nativo e argv do bridge exportam COLORTERM=truecolor. Regressões de ambiente e util passaram; SGR/RGB e desenho já tinham cobertura. | Definir/propagar capacidade de forma consistente no lançamento nativo e bridge host, preservando dotfiles; testar autodetecção real, sem confundir ausência de anúncio com ausência de renderização RGB. |
| M15 | Alta / persistência | **Implementado/testado.** Writer dedicado coalesce snapshots em ordem; CONSISTENT e DURABLE e modo 0600 na criação/substituição. Erro conserva estado válido e v2; retry e reinício real entre processos têm regressões. | Definir flags/política para gravação consistente/durável, ownership de I/O assíncrono e falhas/retry. Testar criação, substituição, erro e último estado válido; manter v2 intacto e não reimportar v2 obsoleto sobre um v3 existente. |

Prioridade restante de lançamento: aceite V01–V04/V06 no ambiente anunciado e execução remota da CI; V05 funcional e baseline Xvfb passaram. M01/M02/M03/M15 já possuem correções e regressões; V07 foi exercitado entre processos nativos. Busca com realce, conflitos, adaptadores e migração já existem; não reabrir esses itens como implementação ausente.

M03/M04/M15 foram implementados em incrementos com regressões específicas de lifecycle, contrato acessível e I/O. O aceite de distribuição/desktop continua separado da evidência nativa.

## Aceite pendente: código existente

| ID | Validação | Onde registrar / critério |
| --- | --- | --- |
| V01 | GNOME Builder | [Checklist GNOME](checklists/gnome-manual.md): abrir manifesto, compilar/executar pela IDE, conferir console e persistência. Build CLI não substitui. |
| V02 | Wayland e entrada real | Na mesma checklist: teclado/IME ibus ou fcitx, CJK, Ctrl+Shift+U, mouse, seleção Unicode, TUI, resize e transferência de abas. Os testes XTest cobrem X11. |
| V03 | Aparência, escala e língua | [Revisão visual](checklists/visual-review.md): aprovação do usuário, fontes/bordas/ícones, pt-BR com textos longos e 125/150/200% no compositor; não confundir com GDK_SCALE sob Xvfb. |
| V04 | Notificações do desktop | [Revisão visual](checklists/visual-review.md): habilitar a preferência, executar `sleep 12; echo done` fora de foco, receber e clicar; testar preferência desligada e aba já fechada. A política/ação têm testes, a entrega GNOME não foi observada. |
| V05 | **Passou no Flatpak/Xvfb, 6/6** | [Agentes](checklists/agents-manual.md) e [GNOME](checklists/gnome-manual.md): PNG/JPEG/clipboard nos dois CLIs pelo bridge host confirmados por códigos visuais. Gestos físicos Wayland seguem separados. Detecção de executáveis não é anexo multimodal. |
| V06 | Orca e aceitação acessível | [Checklist GNOME](checklists/gnome-manual.md): navegação/nomes já podem ser exercitados; texto/caret/seleção/eventos passaram por AT-SPI; falta uso com Orca/Wayland. |
| V07 | Restauração entre processos | [Estabilidade](checklists/stability.md): encerrar e iniciar executáveis distintos, incluindo atualização e erro de gravação, com perfil/XDG de teste isolado. **Passou nativamente:** processos distintos, v2 preservado, prevalência de v3 e erro/retry de migração. Repetir no pacote instalado ao aceitar a distribuição. |

## Expansões de produto fora do MVP

Só priorizar depois dos itens de confiabilidade e aceite:

- Seleção de família/peso da fonte e preferências adicionais com comportamento real.
- Perfis de terminal/por projeto, workspaces nomeados e outros agentes (OpenCode, Hermes, Gemini): não fazem parte do aceite inicial de Claude Code/Codex.
- Eventos de conclusão por agente, quando houver protocolo suportado; não inferir pelo prompt.
- Imagens inline/Kitty/SIXEL no renderer: anexar um caminho ao agente já existe e é outro recurso. Exige estudo do motor, limites, desenho e desempenho.

Essas expansões são propostas, não promessas de implementação nem bloqueios automáticos da primeira versão.

## O que já foi organizado

- Quadro vigente reduzido; os registros anteriores foram preservados em [histórico](history/README.md).
- Evidências P0–P3 reunidas em [relatório datado](reports/2026-11-13-p0-p3.md), sem substituir as checklists de aceite.
- Índice de documentação, guia de desenvolvimento e catálogo de ferramentas.
- Dois backups `*~` movidos sem alterar bytes; builds/caches e novos backups ignorados no Git.
- `inspect_termac.py` passou a ignorar o Ghostty baixado e artefatos locais: a auditoria do frontend não deve confundir a aplicação GTK do upstream com o código do Termac. A revisão do motor continua no wrap e nos headers fixados.

Não houve refatoração do widget, mudança de permissões, remoção de builds ou reinício de sessões pessoais nesta organização. O inventário original foi sucedido pela consolidação: os estados por item acima são vigentes. M07/M08/M13 e expansões não são bloqueadores automáticos do lançamento.

Aceite adicional de 13/11: [pacote instalado/atualizado e geração Orca](reports/2026-11-13-desktop-acceptance.md). A CI remota aguarda repositório de destino; o controle nativo foi tentado e não forneceu árvore verificável para o Builder.
