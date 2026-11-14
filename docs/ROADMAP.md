# Melhorias do Termac

Auditoria de 13/11/2026 sobre o código até `5a14a91`, após P0–P3. **Este é o backlog vigente**, complementado pela revisão arquitetural/estabilidade sobre `51fa0b4`. Os [critérios de estabilidade](checklists/stability.md) definem bloqueadores de lançamento; o backlog inteiro não define. O [plano atual](../PLANO_IMPLEMENTACAO.md) resume o estado; [evidências anteriores](reports/2026-11-13-p0-p3.md) não são substituídas por esta organização.

Tipos: **correção** = comportamento inadequado identificado; **implementação** = capacidade ausente; **validação** = código existe, falta comprovar o aceite. “Alta” indica a ordem recomendada de trabalho, não uma declaração de que todas as versões já falham nesses cenários.

## Ordem recomendada

| ID | Prioridade / tipo | Melhoria e evidência atual | Critério para concluir |
| --- | --- | --- | --- |
| M01 | Alta / correção de QA | **Isolar os cenários de interface e eliminar falsos positivos.** `test_typing_and_interrupt` e `test_large_output` usam a janela compartilhada; carga não confere linhas/código de saída. As capturas P3 mostraram `xecho` e `seq` anexado a caminhos de imagens. `attachments-cleared.png` mostra Geral porque a coleta de widgets também encontra filhos invisíveis. | Limpar ou criar sessão por cenário; usar marcadores de saída que não possam ser satisfeitos pelo comando apenas ecoado; comprovar execução de `seq 1 30000`, retorno ao prompt e ausência de perda de entrada; selecionar Anexos antes da captura; passar cada caso sozinho e em suíte normal/HiDPI. |
| M02 | Alta / correção de produto | **Links alinhados às células Unicode.** `src/termac-util.c:columns_before` conta codepoints; abertura/sublinhado recebem coordenadas de células em `termac-terminal.c`. Reprodução nesta auditoria: `日本語 https://example.org`, coluna física 4, retorna link e início 4; deveria não ser link ali e começar em 7. | Regressões CJK, acento combinado, emoji/ZWJ e seleção; usar geometria real do motor, não substituir por outra estimativa de largura; Ctrl+clique e hover coincidirem com o texto. |
| M03 | Alta / robustez | **Encerrar shells sem bloquear o main loop.** `terminal_stop_child` espera SIGHUP com `g_usleep` por até 1 s e faz espera final após SIGKILL dentro de `dispose`. O caminho síncrono existe no código; a latência sob resistência a SIGHUP ainda não foi medida. | Encerramento assíncrono com ownership explícito de PID/watch/fd; janela responde enquanto o filho ignora SIGHUP; prazo para SIGKILL e coleta sem órfãos/zumbis, inclusive no bridge Flatpak; fechar várias abas não acumula esperas na UI. |
| M04 | Alta / aceite manual | **Texto acessível implementado.** `GtkAccessibleText` expõe viewport/histórico visível, offsets Unicode, limites, caret, seleção, geometria e notificações; [evidência](reports/2026-11-13-accessible-text.md). | Regressão automatizada passou em Xvfb; validar transporte AT-SPI e saída/digitação/seleção no Orca/Wayland. Setters acessíveis retornam `FALSE`, sem alterar o cursor do aplicativo no PTY. |
| M05 | Média / infraestrutura | **CI reproduzível e verificações de organização.** Há testes Meson e scripts, mas nenhum workflow de CI versionado foi encontrado no inventário. | Build com Zig/revisão fixados, testes unitários/UI/HiDPI, schema/Desktop/AppStream, traduções e ambos os perfis; publicar logs/artefatos em falha. Contas reais de agentes e interação do desktop continuam opt-in; não exigir credenciais na CI comum. |
| M06 | Média / medição e desempenho | **Benchmark de 1, 5 e 10 abas.** Há churn/Valgrind, sem matriz atual de inicialização, CPU/RSS, latência, saída extensa e custo da busca. Renderer usa Cairo/Pango por célula. | Após M01, registrar ambiente/build/GPU, amostras comparáveis e picos, foco/segundo plano, busca e resize. Otimizar caches/regiões de desenho ou trocar renderer somente depois de localizar o custo e medir novamente. |
| M07 | Média / implementação | **Busy/idle do shell host no Flatpak.** `terminal_has_job` considera toda sessão host viva ocupada porque não enxerga seus grupos de processos. A confirmação conservadora é intencional e protege trabalho. | Protocolo host confiável de estado/cancelamento, com regressões para prompt, tarefa longa, saída, agente e perda do bridge; preservar a proteção quando o estado for desconhecido. Não ampliar permissões como atalho. |
| M08 | Média / implementação e validação | **Integração de shell além de Bash.** O Bash host informa CWD por OSC 7; outros shells dependem de configuração própria. Conclusão de comandos está disponível na integração Bash opt-in, sem eventos específicos dos agentes. | Escolher um shell por incremento (ex.: Zsh ou Fish), preservar inicialização/dotfiles, validar CWD Unicode/restauração e códigos de saída. Suporte anunciado deve ter cenário real; não inferir estado pelo texto do prompt. |
| M09 | Média / implementação | **Hyperlinks OSC 8 com rótulo.** `terminal_open_url_at` e hover reconhecem texto HTTP(S); o frontend não consulta a URI associada à célula. O motor suporta dados de hyperlink, mas isso não é o mesmo que interface implementada. | Rótulo não URL clicável a partir dos dados do motor, URI/limites validados, ação explícita do usuário, hover/cancelamento/foco corretos e regressões de entrada malformada. Preservar links textuais após M02. |
| M10 | Média / UX e robustez | **Pressão de entrada e falhas comunicadas ao usuário.** P0 já mantém backlog ordenado até 256 MiB, além da fila ativa de 1 MiB. Acima do limite, `terminal_queue_bytes` emite warning e rejeita a entrada, sem feedback na interface. | Política definida para excesso/erro de escrita, feedback traduzido e teste de receptor lento/falha/fechamento; manter ordem e integridade abaixo do limite sem bloquear a UI. |
| M11 | Média / distribuição | **Preparar release verificável.** O manifesto ativo é development; o perfil default compila, mas não há homepage/releases publicados registrados nem aceite desktop completo. | Rodar V01–V06 abaixo, instalar pacote default isolado com ícone/schema próprios, testar atualização/persistência e registrar a versão distribuída. Adicionar URLs/releases apenas quando existirem; não inventar metadata. |
| M12 | Média / correção de ferramenta | **Restringir restart de desenvolvimento à instância correta.** `tools/dev.sh` usa `pgrep/pkill -x termac`, sem distinguir caminho/perfil. Uma instância pessoal/default pode impedir abrir development e ser encerrada por `--restart`. O modo já exige aviso, mas não tem isolamento de processo. | Identificar PID/executável desta raiz e perfil, preservar outras instâncias e confirmar o escopo antes de encerrar; verificar com duas instâncias/projetos isolados. Não testar o restart contra sessões pessoais. |
| M13 | Posterior / manutenção | **Separação incremental de responsabilidades.** `termac-terminal.c` reúne PTY/entrada/renderização; `termac-application.c` reúne sessão/Preferências/atalhos; `test-ui.c` compartilha contexto e cresce. | Primeiro M01; depois extrair uma responsabilidade por vez, com interfaces pequenas, ownership/cancelamento e regressões de lifecycle. Preservar Meson/Builder, caminho de recursos e semântica de sessão. Nenhuma migração de toolkit/motor é necessária para organizar o projeto. |
| M14 | Média / implementação | **Anunciar True Color aos programas.** SGR 24-bit e desenho RGB já foram verificados nesta revisão. O shell define TERM, mas não exporta COLORTERM explicitamente; btop foi exercitado com True Color ativado na configuração. | Definir/propagar capacidade de forma consistente no lançamento nativo e bridge host, preservando dotfiles; testar autodetecção real, sem confundir ausência de anúncio com ausência de renderização RGB. |
| M15 | Alta / persistência | **Primeira gravação consistente/durável sem bloquear a UI.** A migração preserva v2 e agora tem testes de idempotência/falha. `g_file_set_contents` usa CONSISTENT com ONLY_EXISTING; a primeira criação não tem garantia completa contra queda de energia. | Definir flags/política para gravação consistente/durável, ownership de I/O assíncrono e falhas/retry. Testar criação, substituição, erro e último estado válido; manter v2 intacto e não reimportar v2 obsoleto sobre um v3 existente. |

Prioridade de lançamento: M01 primeiro para confiar na evidência; em seguida M03/M15 (encerramento/persistência), V01/V05/V07 (distribuição/reinício) e M04 (acessibilidade). M02 continua uma correção concreta, mas não transforma o Marco 5 inteiro em pré-requisito. Busca com realce, conflitos, adaptadores e migração já existem; não reabrir esses itens como implementação ausente.

M03/M04/M15 precisam de planos próprios por alterarem lifecycle, contrato do widget ou I/O persistente. Nesta revisão foi corrigido o caso de salvar zero janelas e reforçada a migração; isso não conclui todos esses itens.

## Aceite pendente: código existente

| ID | Validação | Onde registrar / critério |
| --- | --- | --- |
| V01 | GNOME Builder | [Checklist GNOME](checklists/gnome-manual.md): abrir manifesto, compilar/executar pela IDE, conferir console e persistência. Build CLI não substitui. |
| V02 | Wayland e entrada real | Na mesma checklist: teclado/IME ibus ou fcitx, CJK, Ctrl+Shift+U, mouse, seleção Unicode, TUI, resize e transferência de abas. Os testes XTest cobrem X11. |
| V03 | Aparência, escala e língua | [Revisão visual](checklists/visual-review.md): aprovação do usuário, fontes/bordas/ícones, pt-BR com textos longos e 125/150/200% no compositor; não confundir com GDK_SCALE sob Xvfb. |
| V04 | Notificações do desktop | [Revisão visual](checklists/visual-review.md): habilitar a preferência, executar `sleep 12; echo done` fora de foco, receber e clicar; testar preferência desligada e aba já fechada. A política/ação têm testes, a entrega GNOME não foi observada. |
| V05 | Imagem real pelo Flatpak | [Agentes](checklists/agents-manual.md) e [GNOME](checklists/gnome-manual.md): PNG/JPEG/clipboard nos dois CLIs pelo bridge do host; confirmar conteúdo visual. Detecção de executáveis não é anexo multimodal. |
| V06 | Orca e aceitação acessível | [Checklist GNOME](checklists/gnome-manual.md): navegação/nomes já podem ser exercitados; saída/caret/seleção dependem de M04. |
| V07 | Restauração entre processos | [Estabilidade](checklists/stability.md): encerrar e iniciar executáveis distintos, incluindo atualização e erro de gravação, com perfil/XDG de teste isolado. A reativação no mesmo processo já tem teste; o ciclo real ainda deve ser aceito. |

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

Não houve refatoração do widget, mudança de permissões, remoção de builds ou reinício de sessões pessoais nesta organização. Os itens M01–M15 continuam abertos; o objetivo desta rodada é organizar e diagnosticar, não declarar essas melhorias implementadas.
