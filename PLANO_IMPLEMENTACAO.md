# Plano de implementação do Termac

Atualizado em 09/11/2026 para C, GTK 4, libadwaita e libghostty-vt.

## Estado atual — 13/11/2026

Este quadro é o ponto de retomada. As seções 1–10 preservam o plano e suas condições no momento em que foi escrito; a seção 11 registra observações históricas. Quando divergirem deste quadro ou do código atual, confira a evidência mais recente antes de agir. O detalhamento técnico está em [PDR.Arquiteture.md](PDR.Arquiteture.md) e as referências visuais em [DESIGN.md](DESIGN.md).

| Marco | Situação | Evidência e trabalho pendente |
| --- | --- | --- |
| 0 — dependência e build | Parcial | Ghostty e Zig continuam fixados. P1: schema instalado com fallback de desenvolvimento; build/exportação/instalação Flatpak CLI com GNOME 50 e árvore atual verificados. Ícones instalados têm canvas quadrado sem alterar fontes. Execução pela interface do Builder continua não verificada; ver resultados P1 abaixo. |
| 1 — shell e PTY | Parcial | Shell e prompt renderizado foram observados em Wayland; o histórico abaixo registra provas anteriores de cores, Unicode e resize. Validar entrada real, TUI, carga, respostas VT, tratamento de falhas e encerramento antes de aceitar o marco. |
| 2 — interação | Parcial | Há GtkIMContext, tratamento de teclas e encoder Ghostty no código. Falta validar entrada composta, teclas e TUI; seleção, copiar/colar, scrollback e política de fechamento ainda estão pendentes. |
| 3 — interface | Parcial | Código de abas, overview, menu, ações, tema, tamanho de fonte e paletas existe. A captura mais recente mostrou o prompt praticamente junto ao topo da área; foi adicionado recuo superior de 24 px no desenho. A janela ativa não foi reiniciada para preservar a sessão; falta conferir visualmente o novo binário, além de múltiplas sessões, overview, atalhos e persistência. |
| 4 — imagens para agentes | MVP validado; marco parcial | P2: seleção explícita e adaptadores de Claude Code/Codex; PNG, JPEG e Ctrl+V confirmados nos dois CLIs reais sob Xvfb (6/6, códigos/cores/formas identificados sem ferramentas). Ver resultados P2. Outros agentes e gestos físicos Wayland não foram validados. |
| 5 — produtividade | Planejado | Busca, links, restauração e notificações permanecem no plano. |
| 6 — consolidação | Parcial | P0 registra estabilidade e recursos; P1 acrescenta auditoria automatizada de nomes/papel acessível, instalação sem build e suíte HiDPI. Conteúdo acessível de texto, Orca, escala fracionária Wayland, aparência dos ícones e medições completas continuam pendentes. |

- As skills locais `gtk`, `libghostty-vt`, `developing-gtk-apps` e `designing-gnome-ui` foram instaladas em `.agents/skills/` e seus `SKILL.md` foram lidos. A revisão estática confirmou GTK 4/libadwaita e diálogos assíncronos; o requisito do Meson agora pede libadwaita 1.8, necessário para `AdwShortcutsDialog`. `meson compile -C build` reconfigurou com libadwaita 1.10.0 e concluiu sem erros; o Ninja não recompilou alvos já atualizados. Não rodei testes; inspeção visual e acessibilidade continuam pendentes.
- As variantes `mhagrelius-dotfiles` de `developing-gtk-apps` e `designing-gnome-ui` também foram instaladas e abertas. Li as referências GTK de padrões, GObject e empacotamento e as referências GNOME de HIG e abas; seus exemplos Python servem como padrões, não como código para transpor literalmente ao app C. Esta instalação não alterou a interface nem acrescentou configuração.
- O desenho do terminal agora soma 24 px ao eixo Y e reduz a altura usada para calcular a grade; `meson compile -C build` foi executado após a mudança. A sessão que já estava aberta permaneceu no binário anterior para preservar o shell e o texto digitado.
- Use as skills como orientação e confira as decisões locais: a skill `libghostty-vt` declara a API C indisponível com base em informação de 2025-09, mas este projeto integra a API C da revisão fixada no wrap. A recomendação de substituir `.dim-label` por `.dimmed` também não foi aplicada: GTK 4 ainda documenta `.dim-label` como classe de `GtkLabel`.
- A base conhecida está em `/home/thiago/Projects/termac`; em 09/11/2026 o Git não tinha commit inicial e a árvore aparecia como não rastreada.
- A faixa `AdwTabBar` usa `autohide=true`: deve ficar oculta com uma aba e aparecer com mais de uma. A confirmação visual de múltiplas abas ainda está pendente.
- Os perfis `development` (`thiago.aciole.Termac.Devel`) e `default` (`thiago.aciole.Termac`) geram IDs, schemas e ícones distintos. Os SVGs originais normal e dev foram copiados para `data/icons`; falta confirmar sua apresentação em todas as superfícies do desktop.
- O Zig 0.16.0 foi instalado em `~/.local/share/zig/0.16.0` e exposto por link em `~/.local/bin/zig`, já presente no PATH; `meson compile -C build` e `./build/src/termac` funcionam sem ajuste de PATH. O app foi relançado no Wayland e a captura enviada pelo usuário mostrou o prompt e os espaçamentos. Um aviso GDK de apresentação encerrou a instância quando `G_DEBUG=fatal-warnings` estava ativo; execução normal não precisa dessa variável.
- A instalação antiga em `~/.local` foi removida dos locais ativos e movida para a Lixeira a pedido do usuário. Preferências pessoais foram preservadas. Não foi localizada instalação Flatpak ou em `/usr/local` na consulta feita na sessão.
- A cópia `/home/thiago/Downloads/termac.old`, derivada do Ptyxis, foi examinada como referência visual e estrutural. Usa VTE/CMake; as medidas e diferenças relevantes estão em [DESIGN.md](DESIGN.md). A análise foi de código, sem validação de execução do app antigo.

- Revisão de erros em 09/11/2026: corrigidas duas leituras de `g_autofree char *percentage` sem inicialização em `src/termac-window.c` (`termac_window_zoom` e `termac_window_sync_settings`), que liberavam ponteiro indefinido ao sair cedo; e o diálogo de atalhos em `src/termac-application.c`, que liberava o builder dono do diálogo antes da apresentação, agora mantido vivo até o sinal `closed`. Verificação: build limpo em diretório temporário e build do projeto com Zig 0.16.0 no PATH sem erros nem warnings; `build/src/termac` iniciado em modo dev no Wayland, permaneceu ativo com log vazio. Não foi feita inspeção visual nem teste de interação; as pendências dos marcos 1–3 continuam em aberto. O validador `tools/validate_skills.py` acusa divergência de `name` em skills de terceiros dentro de `.agents/skills/`, sem relação com o código do app; não foram alteradas.

- Cabeçalho ajustado em 09/11/2026: o botão de visão geral das abas passou para o início da barra e o botão de nova aba foi para o fim, antes do menu principal (`src/termac-window.ui`). Build do projeto sem erros; a mudança visual ainda não foi conferida, pois a instância em execução usa o binário anterior.

- Incremento de conclusão de pendências em 09/11/2026 (código, testes automatizados e um teste real de shell; sem inspeção visual):
  - Motor e entrada (`src/termac-terminal.c`, `src/termac-vt.c`): seleção por mouse com a API de gestos do libghostty-vt, cópia (Ctrl+Shift+C), colagem com bracketed paste (Ctrl+Shift+V), rolagem por roda e por Shift+PageUp/PageDown, relatório de mouse para programas com rastreamento, eventos de foco, prefixo ESC para Alt, Ctrl+Espaço, detecção de tarefa em execução, encerramento com SIGHUP e SIGKILL após 1 s, e falha de inicialização com mensagem e retry por Enter ou clique.
  - Janela (`src/termac-window.c`): renomear aba com título manual preservado, reordenar com ações e arrastar, mover aba para nova janela, subtítulo com o diretório atual (OSC 7), indicador de atenção por campainha e saída em aba fora de foco, confirmação ao fechar aba ou janela com programa em execução, busca no histórico (Ctrl+Shift+F, F3), anexos de imagem por arquivo, clipboard e arrastar e soltar, e diálogo de anexos com copiar caminho e remover.
  - Anexos (`src/termac-attachments.c`): validação de PNG e JPEG pelo cabeçalho, limite de 20 MiB e 8000 px, escrita atômica em cache por sessão, caminho entre aspas para o shell e poda de anexos com mais de 7 dias.
  - Notificações e sessão: conclusão de tarefa fora de foco, restauração das abas (pastas, sem executar comandos) e integração opcional com Bash via OSC 777 (desligada por padrão).
  - Preferências: histórico de linhas, confirmação de fechamento, notificações, integração com shell e restauração de sessão, todas no GSettings.
  - i18n: textos em inglês como msgid, `po/pt_BR.po` com 94 traduções e `LINGUAS` com `pt_BR`; `msgfmt --check` sem erros.
  - Verificação feita: `meson test` com 3 suítes (`util`, `vt`, `attachments`) passando; `vt` cobre seleção ASCII e de caracteres largos, bracketed paste, rastreamento de mouse, busca com rolagem e geração do paste; `attachments` cobre gravação, rejeição, citação e poda. O script de integração com Bash foi executado em um bash interativo sob pty: códigos 1, 0 e 7 reportados corretamente, sem notificações espúrias do prompt. O binário foi iniciado por 8 a 10 s sem falhas, em barramento de sessão isolado.
  - Não verificado: interação real pelo teclado e mouse, aparência da barra de busca, diálogos, notificações no desktop, restauração ao reiniciar, arrastar e soltar, e troca de aba entre janelas. A instância que estava aberta continua com o binário anterior.
  - Não implementado nesta rodada: perfis de terminal, edição de atalhos pela interface (as ações existem e o schema permite ajustar, mas sem editor), escolha explícita de perfil de agente e adaptadores por CLI (Codex, Claude Code e OpenCode estão instalados, mas o anexo nativo exige flags no momento de iniciar o agente, o que não se injeta num prompt já aberto), eventos de conclusão por agente, realce visual dos resultados da busca, medições de desempenho em 1, 5 e 10 abas, verificação de vazamentos (valgrind 3.27.1 instalado em /usr/bin/valgrind; ainda não executado), instalação por usuário, e acessibilidade além do rótulo e papel do widget.

- Ajuste de menu e anexos em 09/11/2026: removidos do menu principal "Colar imagem" e "Anexos"; Ctrl+Shift+V cola imagem como anexo quando o clipboard tem só imagem, e os anexos passaram a uma aba "Anexos" em Preferências (copiar caminho, remover, limpar tudo). Também saíram do menu "Buscar no terminal" e "Mostrar abas abertas", que já têm botões no cabeçalho; os atalhos Ctrl+Shift+F e Ctrl+Shift+O continuam. Verificação: `meson compile -C build` sem erros e `meson test` com 6 testes passando. Não verificado visualmente; a instância aberta usa o binário anterior.

**Próxima retomada sugerida:** validar manualmente no Wayland a entrada pelo teclado, TUI, seleção e colagem com mouse, busca, anexos, renomear e reordenar abas, e as notificações. Depois implementar perfis, edição de atalhos e a integração por agente. Em paralelo, verificar duas ou mais abas, overview e preferências contra as referências. Para build de distribuição, resolver o schema dependente do diretório de build e validar o fluxo real do Builder.

### Prioridades — 13/11/2026

Ordem de trabalho definida pelo usuário. Cada item só fecha com evidência de execução no app, não apenas com build.

- **P0 — Confiabilidade:** validar PTY com vim, IME, rastreamento de mouse, colagem, sessões independentes e restauração. Corrigir falhas reproduzíveis e investigar os vazamentos (valgrind ainda não executado) antes da versão estável.
- **P1 — Integração GNOME:** compilar pelo GNOME Builder, validar o Flatpak, permissões do sandbox, ícone, acessibilidade e escalonamento de interface.
- **P2 — Experiência com agentes:** criar seleção explícita entre Claude Code e Codex, configurar os adaptadores por CLI e validar anexos de imagem. Flags específicas de cada CLI não devem se espalhar pela interface.
- **P3 — Acabamento:** realce de resultados da busca, detecção de conflito de atalhos, notificações e comparação visual com as referências.

Perfis de terminal ficam fora da primeira versão estável (ver PDR, "Decisões de produto").

Os planos de execução estão em `docs/superpowers/plans/2026-11-13-p0-reliability.md`, `...-p1-gnome-integration.md`, `...-p2-agent-experience.md` e `...-p3-polish.md`.

#### P2 — resultados (13/11/2026, branch `p2-agent-experience`)

- **Adaptadores:** `src/termac-agent.c/.h` centralizam IDs, nomes, binários, formatação de anexos, comando inicial e disponibilidade. O registro tem dois agentes e pode crescer por uma linha. A chave GSettings `agent` persiste a escolha em Preferências → Anexos; valor desconhecido usa o padrão sem sobrescrever a configuração. Entradas ausentes continuam selecionáveis e mostram “Não instalado”.
- **Anexos:** clipboard e importação/drop chamam o adaptador; ambos os agentes e o shell simples recebem caminho citado + espaço. Testes passam caminhos com espaço, apóstrofo, `$`, backtick e acento pelo shell e comparam bytes. O teste da interface captura o texto real recebido pelo PTY para os dois agentes, inclusive com PATH sem CLIs. `agent-names-contained` também impede voltar a citar binários ou formatar diretamente fora do adaptador.
- **Lançamento:** ação `win.launch-agent`, menu **Abrir agente** e atalho editável Ctrl+Shift+A, incluído na ajuda. Verifica executável e pasta antes de criar a aba. O comando explícito inicia após o rc do shell interativo e retorna a um shell depois de sair do CLI, evitando colagem de newline durante inicialização/bracketed paste. O teste de rc usa caminho com apóstrofo/espaço e confirma exportação antes do comando.
- **Contexto/robustez:** consultas de disponibilidade são assíncronas, canceláveis e limitadas a 5 s; usam a pasta e o PATH do shell real (host no Flatpak). Revisão independente encontrou checagem na pasta errada e filhos de inicialização sobrevivendo a timeout; ambas foram reproduzidas em testes RED→GREEN. Cada consulta possui sessão própria e limpa os filhos ao concluir, cancelar ou expirar.
- **Pasta no Flatpak:** Bash host passa a emitir OSC 7 automaticamente, com URI codificada byte a byte e preservação do `.bashrc`/PROMPT_COMMAND. A notificação de comandos continua opt-in. O teste no SDK mudou para pasta com espaço, apóstrofo e acentos e conferiu `termac_terminal_get_cwd`; sem esse ajuste permanecia no diretório inicial. Outros shells dependem de seu próprio OSC 7. A confirmação conservadora de fechamento do P1 permanece.
- **Verificação local:** `meson test -C build`: **11/11 suítes passaram**, interface normal 156,00 s e HiDPI 172,74 s. A suíte de agentes tem oito casos (incluindo cancelamento, timeout, filhos e PATH relativo); perfil default também passou em settings/agent/boundary. `msgfmt --check --statistics`: **151 traduções, zero fuzzy**. Novo driver real é opt-in e fica ignorado na CI comum.
- **Flatpak:** build/exportação passaram com GNOME 50. A primeira tentativa em /tmp falhou por cota de disco; apenas os artefatos de teste foram movidos para o workspace ignorado no disco. `/ui/flatpak-agent-context` passou com os dois executáveis reais do host detectados, comando inicial em /tmp, retorno ao shell, CWD Unicode atualizado e nenhuma notificação opt-in disparada. `/ui/flatpak-host-session` e o driver externo também passaram: sleep foi recolhido após fechar a janela. O teste usa um bind temporário exclusivo para tornar o rc/cache de QA visível dos dois lados; não se alteraram permissões do manifesto.
- **Validação real solicitada pelo usuário:** o Computer Use nativo desta sessão estava desabilitado, e a alternativa `agent-cu` local não abriu/capturou o aplicativo. Por solicitação do usuário para automatizar, usei uma instância do próprio Termac em Xvfb com os CLIs reais e seus acessos existentes. **Claude Code 2.1.325 / Sonnet 5.5** e **Codex CLI 0.167.1 / GPT-6.1-Sol medium** reconheceram PNG, JPEG e clipboard Ctrl+V. Cada um identificou K7R4 (quadrado vermelho/círculo azul), M2X9 (triângulo amarelo/retângulo roxo) e V8C6 (triângulo verde/retângulo laranja), presentes apenas nas imagens. Indicadores de anexo e respostas foram observados na grade VT e no PNG renderizado. Não houve chamada de ferramenta nos diálogos. Erros acima de 20 MiB e de 8000 px foram conferidos visualmente nos dois CLIs. Todas as sessões de teste foram encerradas.

Versões/help foram consultados antes de escolher flags: Codex anuncia `-i/--image` para
prompt inicial; Claude não anuncia flag de imagem local (`--file` é recurso remoto).
Nenhuma flag foi necessária para o fluxo interativo de anexo e nenhuma foi adicionada.
A consulta inicial de Claude era 2.1.323; o próprio CLI atualizou para 2.1.325 durante
esta execução, cuja versão/help foram reconferidos.

Procedimento reproduzível, matriz completa e limites da evidência em
[docs/checklists/agents-manual.md](docs/checklists/agents-manual.md).
**Permanece adicional/manual:** gestos físicos e clipboard no compositor Wayland,
GNOME Builder, Orca e escalas fracionárias da checklist P1. Não se declara isso
validado por Xvfb. OpenCode, Hermes, Gemini e eventos de conclusão por agente
continuam fora do MVP/P2.

#### P1 — resultados (13/11/2026, branch `p1-gnome-integration`)

Implementação e testes automatizados concluídos; o aceite visual/manual continua aberto em
[docs/checklists/gnome-manual.md](docs/checklists/gnome-manual.md). Não se declara Builder validado por um build CLI.

- **Settings:** `termac_settings_new` procura primeiro o schema instalado e só depois o build. Três casos isolados em subprocessos cobrem fonte instalada, fallback e ausência. Sem schema o terminal mantém defaults, atalhos padrão e Preferências desabilitadas. `meson install -C build --destdir /tmp/termac-p1-root`, `glib-compile-schemas` e execução do binário instalado por oito segundos com `build/` renomeado passaram sem aviso de schema. Como `dbus-run-session` não existe no host, o smoke isolou o barramento com endereço indisponível: houve o aviso GTK esperado de barramento, sem falha do schema.
- **Flatpak:** instalado `flatpak-builder` 1.4.12 no host. O comando inicial exato do plano falhou em `error: No remote refs found for ‘flathub’`: o SDK 50 está instalado em system, sem remoto user. Usando esse SDK, o manifesto original falhou também em `fatal: ambiguous argument 'master'`. A fonte agora é `dir`, com exclusões de build e ferramentas locais. Corrigido `strip-components=0` no tarball Zig para combinar com o comando existente de instalação. Build, exportação e instalação CLI passaram com alterações ainda sem commit. A instalação foi iniciada com `GDK_BACKEND=wayland` por oito segundos, com GSettings em memória: permaneceu ativa até o timeout (124), log vazio. O Flatpak redefiniu XDG_STATE_HOME para sua pasta privada Devel; o estado do teste ficou nessa pasta. O timeout encerrou o launcher, e o processo gráfico de teste foi encerrado explicitamente depois, sem reiniciar sessão preexistente.
- **Shell:** decisão explícita do usuário pelo host. O runtime informou `/bin/sh`; o host `/bin/bash`. A escolha do shell agora ocorre no host. Um teste revelou que executar Bash diretamente sobre o PTY do sandbox perde controle de tarefas; `script` do util-linux cria o PTY host e preserva interrupção/resize. O teste SDK `/ui/flatpak-host-session` confirmou comando executado fora do sandbox, `pwd=/tmp`, shell correto, retorno após dois Ctrl+C e fechamento. `tools/test-flatpak-host.py` conferiu no host que o PID de sleep foi recolhido após fechar o widget. Pastas restauradas invisíveis no sandbox são preservadas e verificadas no host, com fallback para home se removidas.
- **Permissões:** somente Wayland, fallback-X11, DRI, IPC e D-Bus `org.freedesktop.Flatpak=talk`. A rede foi removida do processo gráfico; o acesso host permite intencionalmente comandos com as permissões normais do usuário. Não há `filesystem=home/host`. Limitação explícita: todo shell host vivo é considerado ocupado para confirmar fechamento; busy/idle exato entre namespaces exigiria outro protocolo. Sem OSC 7, persiste o diretório inicial da aba.
- **Clipboard:** `wl-paste` e `xclip` ausentes no runtime; encontrados no host junto a Claude Code e Codex pelo Bash interativo. Não foi adicionado módulo wl-clipboard porque os CLIs rodam no host. Colagem real de imagem em Claude Code pelo Flatpak ainda não foi verificada.
- **Ícone e metadata:** a primeira exportação rejeitou SVG 250×220 (`Expected a square icon but got: 250x220`). `tools/prepare-icon.py` gera canvas quadrado com margem transparente nos dois perfis; fontes originais intactas. Desktop e SVG Devel foram exportados em `~/.local/share/flatpak/exports/share`. Adicionados Comment, StartupWMClass por ID e categoria TerminalEmulator. Removidos URLs, capturas e release fictícios de example.org. A validação pedante passa com exceção explícita **apenas** para `url-homepage-missing=info`: o projeto ainda não tem homepage pública. Sem essa exceção, permanece um warning de homepage; IDs maiúsculos e releases ausentes são hints preservados/documentados.
- **Acessibilidade:** `/ui/accessibility-labels` começou falhando e passou após nomear busca/fechamento de Preferências e trocar o fechamento interno sem nome da busca por controle próprio rotulado. Confere botões públicos, menus e papel `TERMINAL`; ignora controles invisíveis e botões privados de widgets compostos. `msgfmt --check --statistics po/pt_BR.po`: 135 mensagens traduzidas, zero fuzzy. Isso não comprova leitura do conteúdo por Orca.
- **Escala/testes:** `ui-hidpi` executa o mesmo binário com `GDK_SCALE=2`, `GDK_DPI_SCALE=1.25` e timeout 240. A grade cabe antes/depois do zoom e a escala esperada é conferida. Ajustados testes de seleção/ponteiro para métricas/escala reais e arquivos temporários isolados por suíte: a execução paralela revelou que ambos os testes vi escreviam o mesmo arquivo em /tmp. Não foi encontrado defeito de dupla escala no renderer. Build `default` novo e teste settings passaram; o teste agora depende explicitamente da compilação do schema.
- **Verificação final:** `meson test -C build`: **9/9 suítes passaram**, normal 140,80 s e HiDPI 156,34 s; testes settings no perfil default novo também passaram. Revisão independente encontrou e orientou as correções de shell/PTY/restauração acima. Os testes e a instalação nativa não substituem o aceite visual.

Matriz conferida: host GTK **4.24.1** / libadwaita **1.10.0**; SDK GNOME 50 GTK **4.22.5** / libadwaita **1.9.4**. O mínimo Meson continua libadwaita **1.8**; nenhuma API exclusiva de GNOME 51 foi introduzida.

Viabilidade de texto acessível (spike sem implementação):
1. `gtkaccessibletext.h` existe no GTK instalado; GtkAccessibleText está disponível desde GTK 4.14.
2. `termac_vt_viewport_row_text` fornece linhas que podem alimentar conteúdo/offsets de texto.
3. A interface exige conteúdo, limites, caret, seleção e notificações `gtk_accessible_text_update_*`; não basta mudar o papel.
4. Estimativa preliminar: incremento próprio de 3–5 dias, incluindo testes com Orca e edição/seleção sob scrollback.
5. Riscos: offsets Unicode vs células largas, alterações rápidas de viewport, tela alternativa, scrollback e anúncios excessivos; viável, permanece fora deste P1.

**Não verificado:** GNOME Builder pela IDE, fala do Orca, nitidez/seleção Wayland a 125/150/200%, aparência de dock/alternador/Sobre nos dois perfis e recebimento real de imagens pelos agentes. Nenhum desses itens foi marcado como concluído.

#### P0 — resultados (13/11/2026, branch `p0-reliability`)

Todos os testes novos rodam sob Xvfb com eventos XTest reais (`meson test -C build`, 7 de 7).

- **vi pelo PTY** (`/ui/vim-session`): editar e salvar um arquivo, o PTY acompanha o redimensionamento da janela (`stty size` confere) e fechar a aba com o vi aberto não deixa processo órfão. Passou sem correção.
- **Mouse** (`/ui/mouse-tracking`): clique chega ao programa como relatório SGR; Shift+arrastar seleciona e não é relatado. Passou sem correção.
- **Entrada composta** (`/ui/input-method`): teclas mortas (`á`, `ñ`) funcionam. **Faltava** exibir o texto em composição e informar ao método de entrada a posição do cursor; foram implementados (`termac_terminal_set_preedit`, desenho no cursor, `gtk_im_context_set_cursor_location`). Ctrl+Shift+U não pôde ser verificado no Xvfb.
- **Colagem** (`/ui/paste-integrity`, `/vt/paste/no-end-marker-injection`): **bug encontrado e corrigido** — uma colagem acima de 1 MiB era descartada inteira pelo limite da fila de escrita. Agora o excedente espera num backlog ordenado (limite de 256 MiB) e entra na fila conforme o programa lê. Colagem de 2 MiB chega inteira; o marcador de fim de colagem no texto não encerra a colagem (a libghostty já o remove).
- **Sessões independentes** (`/ui/sessions-isolated`): variável de ambiente, pasta e texto digitado não vazam entre abas. Passou sem correção.
- **Restauração** (`/ui/session-restore-roundtrip`, `/ui/session-corrupt-file`): **bug encontrado e corrigido** — uma aba que nunca recebeu OSC 7 era salva com a pasta pessoal, então abas restauradas perdiam a pasta ao salvar de novo. `termac_terminal_get_cwd` agora lê `/proc/<pid>/cwd` quando não há relatório do shell. Arquivos corrompidos, vazios e de tipo errado não impedem a abertura.
- **Vazamentos** (`tools/valgrind-churn.sh`, `tools/valgrind.supp`): o valgrind não executa o código SIMD da libghostty-vt (SIGILL), então o script usa uma árvore `build-valgrind` com `-Dsimd=false`. Com 3 e com 15 ciclos de abrir e fechar aba, **vazamento real encontrado**: 1 referência de `GThemedIcon` por aba (`termac_window_create_tab`, ≈163 bytes por aba; crescia de 4 para 16 blocos). Corrigido; depois da correção os dois tamanhos dão exatamente 22.689 bytes em 128 blocos, sem frames do Termac. O restante é de bibliotecas e constante (fontconfig `FcFontSetList`, Pango, GDK GL), suprimido em `tools/valgrind.supp`. O vazamento de `pango_context_get_metrics` de 12/11 era cache do Pango, constante.
- **Não verificado:** motores de entrada reais (ibus, fcitx, CJK) e Ctrl+Shift+U; comportamento no Wayland; o teste do vazamento de ícone é a execução do valgrind, não um caso automatizado em `meson test`.

### Atualização — 12/11/2026

- **Anexo por arrastar (Marco 4, parcial):** verificado pelo usuário no app de dev, em Wayland. Arrastar um PNG para o terminal anexa a imagem ao prompt do Claude Code e do Codex. O Codex mostra `[Image #1]` com o aviso "Image attached to the prompt". O arquivo foi gravado no cache da sessão (`~/.cache/termac/attachments/`), conferido em disco.
- **Correção do arrastar:** o handler de soltura (`window_drop_files`, `src/termac-window.c`) não recebia a lista de arquivos porque a verificação `G_VALUE_HOLDS_OBJECT` retornava falso para `GdkFileList`. Agora usa `G_VALUE_HOLDS` e `g_value_peek_pointer`. A causa exata da falha da macro não foi investigada.
- **Ctrl+V nativo:** o Claude Code leu a imagem do clipboard quando `wl-clipboard` estava instalado. Hipótese: as ferramentas de clipboard são dependência do caminho nativo, inclusive dentro do Flatpak.
- **Atalhos (confirmados pelo usuário no app de dev, 12/11):** Ctrl+C copia a seleção; sem seleção, duas pressões em até 1 s enviam `^C`. Ctrl+V cola texto ou anexa imagem quando o clipboard só tem imagem. Ctrl+A seleciona todo o conteúdo. Ctrl+Seta para a esquerda envia Home. Ctrl+Shift+C e Ctrl+Shift+V continuam. Conflitos conhecidos: Ctrl+C e Ctrl+V do Claude Code, Ctrl+V do vim e salto por palavra com Ctrl+Seta.
- **Ctrl+V no Claude Code (confirmado, 12/11):** com o Termac interceptando o Ctrl+V e o clipboard contendo imagem, o Claude Code recebeu o anexo; a captura mostra o aviso "Image attached to the prompt" no prompt do Claude Code.
- **Preferências (visual não conferido):** diálogo em janela separada, transitória da janela principal, com páginas Geral, Aparência, Atalhos e Anexos. Busca na barra do cabeçalho. Botão "Mostrar todas as paletas" (9 temas, depois 12). Cartões sem texto de exemplo, por decisão do usuário. Grupos da página Geral: Inicialização, Terminal, Integração e Segurança, só com opções implementadas.
- **Anexos:** miniaturas quadradas de 64×64 px, recortadas do centro, com cantos arredondados. Nome e caminho com o caminho cortado em uma linha. "Clear All" no rodapé.
- **Traduções:** `po/pt_BR.po` com 125 mensagens traduzidas. Restam `translator-credits`, que é do tradutor, e a confirmação visual das telas em português.
- **Verificação:** `meson compile -C build` sem erros; `meson test -C build` com 6 testes passando; app de dev reiniciado a cada mudança. Não houve inspeção visual das preferências nem dos atalhos.
- **Pendências desta rodada:** validar dependência de `wl-clipboard` no Flatpak; avisos de compilação (`gdk_texture_new_for_pixbuf` obsoleto e declarações mistas em `terminal_command_finished_cb`). OpenCode foi retirado do MVP por decisão do usuário em 12/11/2026. Hermes e Gemini não estão instalados.

### Implementação — 12/11/2026 (continuação)

- **Falha de inicialização (Marco 1):** já existia. O terminal mostra a mensagem de erro com "Press Enter or click to try again" (`src/termac-terminal.c`, desenho do erro). Não foi testado com falha real.
- **Links clicáveis (Marco 5):** já existia. Ctrl+clique abre a URL do texto da linha (`terminal_open_url_at`, `termac_url_at_column`). Não há sublinhado ao passar o mouse. Não foi testado pelo usuário.
- **Salvar e restaurar workspace (Marco 5):** a sessão já guardava ordem, pastas, títulos e aba selecionada. Agora guarda também largura, altura e estado maximizado de cada janela, no arquivo `session-v3.gvariant`. A v2 deixa de ser lida, então a primeira abertura depois dessa mudança começa sem as abas anteriores. Comandos nunca são salvos nem executados, então não há confirmação a fazer.
- **Acessibilidade (Marco 6):** rótulos acessíveis nos botões só com ícone da barra (visão geral, busca, menu e nova aba) e nos botões de copiar e remover anexo. Não foi testado com leitor de tela.
- **Editor de atalhos (Marco 5):** dez ações de janela e aplicativo podem ser trocadas em Preferências, em Atalhos, grupo Aplicativo. Cada troca é guardada na chave `action-shortcuts` do GSettings. "Restaurar padrões" limpa as trocas. Os atalhos de terminal (Ctrl+C, Ctrl+V, Ctrl+A, Ctrl+Seta para a esquerda) continuam fixos. Não há detecção de conflito entre atalhos. Não foi testado visualmente.
- **Traduções:** os textos novos do editor de atalhos e da barra de busca foram traduzidos; restam `translator-credits` (do tradutor) e a conferência visual.
- **Verificação:** `meson compile -C build` sem avisos de código do projeto; `meson test -C build` com 6 testes passando; app de dev reiniciado.

### Testes automatizados de interface — 12/11/2026

- Novo teste `tests/test-ui.c` (`meson test -C build ui`) roda o Termac real dentro de um Xvfb, com diretórios XDG temporários e GSettings em memória, sem tocar na sessão do usuário.
- Casos: janela com uma aba; nova aba e fechar aba; 10 ciclos de abrir e fechar abas; abertura de Preferências; salvamento da sessão em `session-v3.gvariant`; digitação real pelo servidor X (XTest, carregado com `dlopen`, `libXtst.so.6`), com comando executado no shell; `sleep 30` interrompido por Ctrl+C de duas pressões (uma pressão não interrompe, duas interrompem).
- Capturas: com `TERMAC_UI_SCREENSHOTS=<pasta>` o teste grava PNGs da janela principal, da digitação, da interrupção e das Preferências, renderizados pelo próprio GTK.
- Limitações: o teste não cobre arrastar e soltar, Ctrl+V com clipboard de imagem, IME, editor de atalhos, os agentes externos nem o visual das abas de Preferências além de Geral. Rodar o valgrind sobre o teste derrubou o processo com "Illegal instruction", então a verificação de vazamento ainda usa o app.
- Verificação: `meson test -C build` com 7 testes passando (inclui validação de arquivos desktop, appstream e schema).

### Automação de interface — continuação (12/11/2026)

- `tests/test-ui.c` agora cobre também: arrastar imagem para o terminal (chama `termac_window_attach_file_list`, a mesma função do sinal de soltura; o disparo do sinal GTK sem evento real não funciona), Ctrl+V com imagem no clipboard (passa: anexo criado), seleção com mouse pelo XTest (roda, mas a cópia não gerou texto no clipboard) e editor de atalhos (falha).
- Falha aberta 1, editor de atalhos: após clicar no botão de "Nova aba" e enviar Ctrl+Shift+Y, o atalho continua `<Control><Shift>t`. Possível causa: o diálogo de captura não recebe o foco do teclado virtual. Não investigado além disso.
- Achado aberto 2, cópia de seleção com mouse: a seleção aparece na captura, mas `termac_terminal_copy` retorna FALSE, então o clipboard não recebe texto. Pode ser bug real ou da forma como o teste faz o arrasto. Não resolvido.
- Limitações sem automação aqui: agentes externos (Claude Code, Codex) e IME.
- `meson test -C build`: 6 de 7 passam; `ui` falha no editor de atalhos.

### Plano de correção — execução (12/11/2026)

- **Editor de atalhos:** o app funciona; o teste falhava por causa do ambiente. O editor só recebe as teclas quando é a janela de cima. Quando a janela principal fica por cima, o teste perde as teclas. Correção aplicada no teste: o caso roda primeiro. Não há mudança de comportamento no app. Comparação de atalhos passou a ser semântica (mesma tecla e modificadores, independente de ordem e caixa).
- **Cópia por seleção:** o gesto de seleção do motor e Ctrl+Shift+C copiam "termac-ok" para o clipboard. O teste alimenta o gesto pela API VT porque o ponteiro do XTest não chega ao GTK neste Xvfb. O clique real do mouse continua sem teste automatizado.
- **Arrastar imagem:** a lógica foi separada em `termac_window_attach_file_list`, chamada pelo sinal de soltura. O teste chama a mesma função.
- **Ctrl+V com imagem:** passa no teste.
- **Verificação:** `meson test -C build` com 7 de 7 passando.
- **Pendente da fase 1:** valgrind sobre o teste (ainda derruba com "Illegal instruction"). Ainda não tentado nesta rodada.

### Fase 2 da correção — 13/11/2026

- **Causa raiz corrigida (vazamento de terminal e shell órfão):** o terminal de uma aba fechada não era liberado. O `GtkIMContext` mantinha o widget como cliente, e o terminal mantinha o contexto, formando um ciclo. Com isso, `dispose` nunca rodava e o shell da aba continuava vivo. Correção em `src/termac-terminal.c`: o widget cliente só fica definido enquanto o terminal está realizado (`realize`/`unrealize`). Teste `closed-tab-releases-terminal` (referência fraca) e `no-orphan-shells` passam.
- **Memória sob churn:** 40 ciclos de abrir e fechar abas, sem crescimento (401 MB antes, 400 MB depois). Teste `memory-churn`.
- **Saída grande:** `seq 1 30000` termina e o prompt volta. Teste `large-output`.
- **TUI:** `less` entra na tela alternativa e sai ao pressionar q. Teste `tui-alternate-screen`.
- **Abas independentes:** dois terminais, com shells próprios; fechar uma mantém a outra. Teste `tabs-independent`.
- **Geometria salva:** o arquivo `session-v3.gvariant` guarda largura e altura. Teste `geometry-saved`.
- **Diálogo de atalhos:** a lista usa `action-name`, então acompanha as combinações editadas. Nenhuma mudança necessária.
- **Verificação:** `meson test -C build` com 7 de 7 passando.
- **Tema, fonte e zoom com três abas (13/11/2026):** teste `theme-zoom-with-tabs`. Clica os cartões de tema (sistema, claro, escuro) e confere `AdwStyleManager`; aciona zoom (+3, −1) e confere o tamanho de fonte de todas as abas e o rótulo de porcentagem. Foi preciso o acessor de leitura `termac_terminal_get_font_size`. Nenhum defeito encontrado no app.
- **Anexos em sequência (13/11/2026):** teste `attachment-sequence`. Anexa duas imagens por arrastar, confere +2 no cache; remove uma pelo botão "Remover" da página Anexos de Preferências (+1); usa "Clear All" (0) e confere o estado vazio. Passa.
- **Ainda não feito na fase 2:** verificação com leitor de tela; detecção de conflito entre atalhos; decisão sobre a migração da sessão v2; vim não automatizado; paleta não é conferida por leitura de estado (só se clica o cartão, sem teste de cor aplicada); as capturas dos novos testes só são gravadas com `TERMAC_UI_SCREENSHOTS`, e não as inspecionei visualmente.
- **Verificação (13/11/2026):** `meson compile -C build` sem erros; `meson test -C build` com 7 de 7 suítes passando (`ui` com 18 casos, 73 s).
- **Atalhos do terminal editáveis (13/11/2026):** Ctrl+C (copiar ou cancelar comando), Ctrl+V (colar) e Ctrl+Seta esquerda (início da linha) agora são ações de janela `win.terminal-copy`, `win.terminal-paste` e `win.terminal-line-start`. Aparecem no editor de Preferências > Atalhos > Atalhos editáveis, com "Restaurar padrões", e na ajuda de atalhos. O terminal só deixa passar essas teclas quando a janela as reconhece; por isso reatribuir Ctrl+C libera a tecla, que então segue ao programa como ^C. Ctrl+Shift+C/V e Ctrl+A continuam fixos e aparecem como "não editável". Mudanças: `src/termac-terminal.c` (envio de tecla nomeada extraído em `terminal_send_key`, atalhos fixos removidos), `src/termac-window.c` (ações e lista de reservados), `src/termac-application.c` (editor e linhas de Preferências), `src/shortcuts-dialog.ui`, e `po/pt_BR.po` (tradução completa, sem fuzzy).
- **Correção encontrada no editor:** a janela de captura de atalho era anexada à janela ativa, e não à janela de Preferências que contém o botão. Agora é anexada à janela do botão (`shortcut_edit_clicked_cb`).
- **Teste novo:** `/ui/terminal-shortcut-rebind` reatribui Ctrl+C para Ctrl+Shift+Y, confirma que um `sleep 30` é interrompido por um único Ctrl+C e restaura o padrão. O teste de colar imagem (`/ui/paste-image-clipboard`) continua usando Ctrl+V padrão e passa.
- **Verificação (13/11/2026):** `meson test -C build`: 7 de 7 suítes passando (`ui` com 19 casos, 81 s). `msgfmt --statistics`: 126 traduzidas, sem fuzzy; restam `translator-credits` (do tradutor) e a entrada do cabeçalho.
- **Incremento de funcionalidades (13/11/2026):**
  - **Ctrl+A editável:** agora é `win.terminal-select-all`, na mesma lista de atalhos editáveis; a ajuda também usa a ação.
  - **Seleção termina ao digitar:** `terminal_send_input` limpa a seleção (`termac_vt_clear_selection`). Antes, um Ctrl+C depois de copiar copiava de novo em vez de interromper o comando.
  - **Links com Ctrl:** com Ctrl pressionado sobre uma URL, o cursor vira mão e a URL ganha sublinhado (`terminal_update_link_hover`). Sai ao mover o ponteiro sem Ctrl ou ao sair do terminal. Novo `termac_url_span_at_column`, com colunas de início e fim; teste unitário `/util/url-span`.
  - **Teste novo:** `/ui/select-all-shortcut` confere o atalho, o Ctrl+A e que digitar limpa a seleção.
  - **Aviso de compilação:** `tests/test-vt.c` tinha declaração misturada com código; corrigido. O build está sem avisos.
  - **Verificação:** `meson test -C build` com 7 de 7 suítes (`ui` com 20 casos).
  - **Não coberto por teste:** o sublinhado e o cursor de mão. O ponteiro do XTest não chega ao GTK neste ambiente, então isso precisa de conferência manual.
  - **Ainda pendente:** realce dos resultados da busca. Hoje a busca só rola até a linha; destacar cada ocorrência exige mapear colunas e acompanhar a rolagem.
- **Pendente desta mudança:** não conferi visualmente a linha "Atalhos editáveis" nem a ajuda; o editor de Ctrl+Seta e Ctrl+C ainda precisa ser testado com a versão de dev em uso real, inclusive a diferença entre Ctrl+C e o cancelamento por duas pressões agora que o padrão é uma ação.

### Decisões de produto e migração — 13/11/2026

- **Ctrl+Shift+C e Ctrl+Shift+V editáveis:** novas ações `win.terminal-copy-selection` (copiar seleção, sem ^C) e `win.terminal-paste-clipboard` (colar), com os mesmos padrões. Aparecem em Preferências > Atalhos > Atalhos editáveis e na ajuda de atalhos. A linha fixa "Copiar e colar (não editável)" e o grupo "Terminal" de Atalhos foram removidos, e o atalho fixo do terminal saiu de `src/termac-terminal.c`.
- **Sessão v2 → v3:** `migrate_v2_session` (`src/termac-application.c`) converte `session-v2.gvariant` em v3 uma única vez, quando o v3 não existe. Abas e pastas são preservadas; a geometria recebe o padrão. O v2 não é apagado e não é lido depois da migração. Teste `/ui/session-v2-migration`.
- **Atenção nesta máquina:** o `session-v3.gvariant` já existe (gravado em 13/11/2026 às 05:15), então a migração **não roda** neste ambiente. As abas da v2 de 12/11/2026 seguem no disco e não foram restauradas. Ver pendências.
- **PDR.Arquiteture.md:** OpenCode removido; Claude Code e Codex CLI como prioridade do MVP de agentes; decisões de produto registradas.
- **Créditos:** `translator-credits` fica vazio no `.po` para tradutores. Autoria e créditos de desenvolvimento estão em `developers` e `developer-name` no diálogo Sobre (`src/termac-application.c`); nada de desenvolvimento usa `translator-credits`.
- **Perfis de terminal:** fora do escopo da primeira versão estável.
- **Traduções:** `po/pt_BR.po` com 129 mensagens traduzidas e sem aproximações (fuzzy). Duas entradas novas, "Copy selection" e "Paste from clipboard", foram conferidas à mão, pois o `msgmerge` as marcou como aproximadas e sugeriu traduções de outras frases. `msgid "Copy and paste (not editable)"` ficou obsoleta.
- **Verificação:** `meson test -C build` com 7 de 7 suítes. `ui` com 22 casos, incluindo `/ui/clipboard-shortcuts-default` e `/ui/session-v2-migration`. O build do app não tem avisos. `tests/test-ui.c` ainda tem três avisos de API X11 depreciada da GTK 4.24, não tratados nesta rodada.
- **Não verificado visualmente:** a nova linha de Ctrl+Shift+C/V em Atalhos e a ajuda de atalhos.

### Verificação com valgrind — 12/11/2026

- Executado com `valgrind --leak-check=full` (valgrind 3.27.1, `/usr/bin/valgrind`), uma janela com uma aba, abertura de Preferências e saída pelo menu. Log em `scratchpad/valgrind-115337.log`.
- Erros de acesso inválido e leitura não inicializada: nenhum.
- `Mismatched aligned_alloc/free` (2 ocorrências): originado em `libadwaita`, não no código do Termac.
- Vazamentos definitivos atribuídos ao Termac: cerca de 30 KB em `termac_terminal_init` e 10 KB em `termac_terminal_set_font_size`, na linha de `pango_context_get_metrics`. O `PangoFontMetrics` é liberado na mesma função. Ainda não é claro se a retenção é do Pango ou de uso nosso; precisa de uma execução mais longa com criação e fechamento de abas para medir crescimento.
- Revisão feita: `pango_font_description_free` já existia nas duas funções; uma edição temporária foi desfeita e o arquivo ficou como estava.
- Pendente: rodar com várias abas abertas e fechadas, e medir crescimento de memória.

### Mapeamento para conclusão — 12/11/2026

Escopo de conclusão: uso diário, com o MVP de agentes formado por Claude Code e Codex. OpenCode saiu do MVP; Hermes e Gemini ficam fora da validação do MVP.

- **Marco 0:** falta observar o build pelo GNOME Builder e validar o build Flatpak com `wl-clipboard` no sandbox.
- **Marco 1:** falta validar TUI (vim, less), saída extensa, respostas VT e encerramento durante I/O; falta mensagem de recuperação na falha de inicialização.
- **Marco 2:** falta validar texto composto (IME, ABNT2), mouse em programas com rastreamento, seleção com caracteres largos e emoji, e bracketed paste em uso. Os atalhos novos já foram confirmados pelo usuário.
- **Marco 3:** falta validar três sessões independentes, overview, persistência após reinício, comparação visual com as referências, ícone no desktop e a ajuda de teclado.
- **Marco 4:** falta confirmar que o Codex descreve a imagem anexada. Claude Code já anexou por arrastar e Ctrl+V.
- **Marco 5:** falta validar busca, notificações e transferência de aba em uso. Links clicáveis, editor de atalhos, eventos de conclusão por agente e salvar workspace não estão implementados; perfis foram removidos por enquanto.
- **Marco 6:** falta acessibilidade além de rótulos, testes em X11 e múltiplas escalas, medições com 1, 5 e 10 abas, verificação de vazamentos com valgrind (instalado: valgrind 3.27.1 em /usr/bin/valgrind), comparação final com as referências e instalação por usuário.
- **Decisões tomadas:** OpenCode fora do MVP. Janelas e abas entram em Geral quando tiverem opções. Os créditos de tradução (`translator-credits`) ficam com o usuário.

### Atualização do Marco 0 — versão do runtime (09/11/2026)

- Por decisão do usuário, o manifesto `thiago.aciole.termac.json` permanece em runtime/SDK GNOME 50 para ampliar compatibilidade. O SDK 51 instalado durante a tentativa anterior foi preservado.
- `flatpak info` confirmou `org.gnome.Platform//51` e `org.gnome.Sdk//51` instalados; antes da alteração, `org.gnome.Sdk//51` precisou ser instalado para avaliar o alvo 51.
- `meson compile -C build` com Zig 0.16.0 terminou com `ninja: no work to do`; o JSON do manifesto foi validado.
- `flatpak run org.gnome.Builder --version` identificou GNOME Builder 50.0. `--project` recebe a pasta raiz do projeto, não o manifesto JSON. A invocação `flatpak run --command=gnome-builder org.gnome.Builder --project=/home/thiago/Projects/termac` inicializou a IDE sem erro no terminal.
- A mensagem `Build directory .../staging/x86_64-main não inicializado` correspondia a um staging Flatpak vazio. Inicializei esse diretório com `flatpak build-init --arch=x86_64 ... thiago.aciole.Termac.Devel org.gnome.Sdk//50 org.gnome.Platform//50 main`; a execução subsequente de `flatpak build <staging> /usr/bin/true` terminou sem erro.
- O staging está pronto para uma nova tentativa do build do projeto no Builder. O comando inofensivo confirma apenas que Flatpak reconhece o diretório; a compilação e a execução do Termac continuam pendentes.

## 1. Objetivo e decisões consolidadas

Construir um terminal Linux próprio, com a interface das quatro referências fornecidas e integração com Codex, Claude Code, Hermes, OpenCode e Gemini. Priorizar entregas executáveis pequenas, começando por um shell funcional na janela GTK.

- Linguagem da aplicação em C.
- Interface em GTK 4 e libadwaita, aproveitando a base do GNOME Builder.
- Motor de emulação em libghostty-vt, pela API C pública.
- Build inicial com Meson, preservando o fluxo gerado pela IDE. O build da biblioteca será integrado conforme o procedimento da revisão fixada do Ghostty.
- Um terminal por aba.
- Imagens como anexos de entrada para os agentes.
- Inglês como idioma fonte e tradução pt-BR.
- Execução nativa no Linux, com shell e ferramentas do ambiente do usuário.
- Identificador solicitado `thiago.aciole.Termac`.

O prazo de uma hora será usado como primeira janela de investigação e integração. A estimativa do MVP depende principalmente do renderer GTK e da qualidade da interação com aplicações TUI.

## 2. Base e localização verificadas

A base gerada está em `/home/thiago/Projects/termac`. O diretório anteriormente informado `/home/thiago/Projetos/termac` está ausente. O plano usa a base encontrada como local de trabalho provisório. Resolver o caminho definitivo antes de movimentar arquivos.

A base contém C, arquivos GtkBuilder XML, Meson, GResources, GSettings vazio, gettext e uma janela de exemplo. Ainda utiliza `thiago.aciole.termac`, com inicial minúscula. O manifesto gerado pela IDE usa Flatpak e precisará de adequação ao fluxo nativo.

Meson foi localizado em `/usr/bin/meson`. Zig ainda precisa ser localizado ou provisionado na etapa de preparação, pois a consulta ao PATH não encontrou o executável. Instalações de ferramentas e versões serão tratadas na implementação.

A elaboração deste plano incluiu leitura de arquivos e documentação. A compatibilidade do motor com este projeto ainda será validada pelos primeiros marcos.

## 3. Identidade e referências visuais

### Ícone oficial

Usar `/home/thiago/Documentos/termac/icons/icon.svg`, fornecido pelo usuário, como origem do ícone do aplicativo. O arquivo existe e declara dimensões de 250 × 220.

Na implementação, incluí-lo em `data/icons/hicolor/scalable/apps/thiago.aciole.Termac.svg` e configurar application ID, arquivo desktop e janela para o mesmo nome. Preservar a proporção original e conferir apresentação no launcher, alternador de janelas e diálogo Sobre. Se o ambiente precisar de uma tela quadrada para instalação, centralizar a arte com margens proporcionais, preservando o desenho.

### Quatro telas de referência

Diretório `/home/thiago/Documentos/termac/screenshoot`.

| Imagem | Conteúdo esperado | Implementação |
| --- | --- | --- |
| Frame 3.png | Cabeçalho escuro, título central, nova aba, menu, controles de janela, barra de abas e área de terminal | AdwApplicationWindow, AdwToolbarView, AdwHeaderBar, AdwTabBar e widget de terminal próprio |
| Frame 3 2.png | Menu com tema, zoom, nova aba/janela, visão geral, tela cheia, preferências, atalhos e Sobre | GtkPopover, GActions e AdwStyleManager |
| Frame 3 3.png | Grade de miniaturas, busca, contador, fechar e criar abas | AdwTabOverview ligado ao mesmo AdwTabView |
| Frame 3 4.png | Preferências em aparência, comportamento, atalhos e perfis | Componentes de preferências libadwaita e GSettings |

Conferir espaçamentos, contraste, posição do menu, aba ativa e controles em tamanho lógico equivalente às referências. Manter os textos e estados dos controles coerentes com os recursos entregues. Definir o número exato de opções das preferências por incremento.

## 4. Fronteira entre motor e aplicação

A API pública libghostty-vt fornece parsing de sequências VT, estado do terminal e dados para o renderer. O aplicativo implementa desenho, entrada e conexão ao processo. A documentação atual informa que as assinaturas da API ainda estão evoluindo. Fixar uma revisão e a versão compatível do compilador Zig torna a integração reproduzível. [Ghostty](https://github.com/ghostty-org/ghostty/blob/main/README.md)

O Ghostling é um exemplo oficial mínimo em C usando Raylib, útil para entender a API e o fluxo do terminal. Seu renderer e sua janela precisarão de adaptação ao GTK. Avaliar cada trecho aproveitado, registrar a origem e cumprir as condições da licença correspondente. [Ghostling](https://github.com/ghostty-org/ghostling/blob/main/README.md)

### Componentes do Termac

| Componente | Responsabilidade |
| --- | --- |
| TermacApplication | Ações globais, janelas e ciclo de vida |
| TermacWindow | Cabeçalho, abas, menu e foco |
| TermacTerminal | Widget GTK, conexão entre estado, renderer, entrada e sessão |
| TermacSession | PTY, shell, filas de I/O, tamanho e processo filho |
| TermacRenderer | Layout e desenho do conteúdo do motor |
| TermacAttachments | Clipboard, arquivos, validação e cache de imagens |
| TermacAgent | Método de anexação e perfil escolhido para cada CLI |
| TermacPreferences | Ligação entre preferências e GSettings |
| TermacWorkspace | Persistência de organização e diretórios das abas |

Criar cada componente quando seu marco entrar em execução. Separar estado, renderer e PTY desde a primeira prova facilita diagnosticar falhas e evoluir o desenho.

## 5. Estratégia de renderização

**Escolha inicial da prova.** O primeiro widget usa `GtkDrawingArea` com Cairo e PangoCairo para validar rapidamente a extração de células do render-state, cores, cursor e shaping Unicode. Isso difere da proposta inicial de snapshot/GSK; medir o desempenho e migrar para snapshot ou GtkGLArea se a carga real justificar.

- Mapear células e atributos do motor para posições consistentes na grade.
- Desenhar fundos, texto, cursor, seleção e decorações.
- Respeitar graphemes, caracteres largos, marcas combinantes e fallback de fontes.
- Desenhar trechos contíguos e reaproveitar layouts onde possível, com cache limitado e invalidação quando fonte, escala ou cores mudarem.
- Desenhar regiões visíveis e solicitar atualizações quando o estado mudar.
- Tratar mudança de escala, fonte, zoom, tamanho e foco da janela.
- Manter snapshot compatível com as miniaturas do AdwTabOverview.

Conferir a API real da revisão selecionada antes de fechar as estruturas internas. Aceleracão e latência equivalentes às do aplicativo Ghostty dependem de implementação e medições próprias. Se o renderer inicial se mostrar insuficiente, usar os resultados para decidir uma implementação específica com GtkGLArea e atlas de glifos em um marco posterior.

## 6. Sequência de implementação

### Marco 0 — Dependência fixada e ligação C

- [ ] Registrar a revisão de libghostty-vt e a versão de Zig requerida.
- [ ] Conferir a API C pública, exemplos de inicialização, liberação, render state, entrada e callbacks.
- [ ] Definir dependência local ou download com revisão fixa e checksum, conforme a distribuição disponível.
- [ ] Integrar o build ao Meson com artefatos e dependências declarados.
- [ ] Configurar build e execução nativos no GNOME Builder.
- [ ] Padronizar application ID, schema, recursos, desktop e ícone.
- [ ] Validar criação e destruição de um estado terminal, alimentação de bytes e leitura do estado visível.

**Aceite.** Build reproduzível, aplicativo GTK abrindo e API C vinculada. Documentar os comandos exatos comprovados no ambiente.

### Marco 1 — Shell, PTY e primeira tela

- [ ] Criar um PTY e iniciar o shell do usuário com diretório e ambiente apropriados.
- [ ] Fazer o filho estabelecer a sessão e o terminal controlador corretamente.
- [ ] Integrar leitura e escrita não bloqueantes ao main loop da GLib.
- [ ] Suportar escritas parciais, EINTR, EAGAIN, EOF/EIO e filas limitadas com controle de pressão.
- [ ] Alimentar o motor com a saída do shell e encaminhar respostas VT pelo canal de escrita do PTY.
- [ ] Limitar trabalho por iteração para manter a interface responsiva sob muita saída.
- [ ] Implementar o renderer inicial com texto, fundos, cursor e redimensionamento.
- [ ] Calcular linhas/colunas e sincronizar tamanho do motor e do PTY.
- [ ] Registrar e reaproveitar o término do filho, removendo fontes de I/O e callbacks ao fechar.
- [ ] Exibir falhas de inicialização com ação de recuperação.
- [ ] Configurar TERM compatível com as capacidades realmente implementadas e com o terminfo disponível.

**Aceite.** Abrir o shell e executar comandos interativos, mostrar cores e acentos, tratar saída extensa e redimensionar a janela sem corromper a grade. O terminal responde a consultas e a interface continua operável.

### Marco 2 — Entrada, seleção e interação confiáveis

- [ ] Integrar GtkIMContext para texto composto e métodos de entrada.
- [ ] Encaminhar teclas especiais, modificadores e eventos de teclado conforme a API e os modos do motor.
- [ ] Tratar modos de mouse, foco e tela alternativa.
- [ ] Implementar seleção de texto, copiar e colar.
- [ ] Respeitar bracketed paste conforme o estado do terminal.
- [ ] Implementar scrollback, ajuste de rolagem e retorno ao conteúdo recente.
- [ ] Validar caracteres largos, emoji e combinantes em seleção e redimensionamento.
- [ ] Confirmar fechamento quando houver tarefa ativa, com política explícita para terminar a sessão e aguardar o processo.
- [ ] Preservar atalhos usados pelos agentes e permitir encaminhar Ctrl+C, Ctrl+D e Ctrl+V quando aplicável.

**Aceite.** Shell, editor em tela cheia e ao menos um CLI de agente funcionam com texto, navegação, interrupção, colagem e redimensionamento. Fechar a janela libera o PTY e recolhe o processo filho.

### Marco 3 — Interface das referências

- [ ] Usar AdwTabView e AdwTabBar, com uma sessão independente por aba.
- [ ] Implementar criar, selecionar, fechar, renomear e reordenar abas.
- [ ] Atualizar título e diretório pelos eventos disponibilizados pelo motor e pelo shell.
- [ ] Preservar título manual até o usuário voltar ao modo automático.
- [ ] Implementar nova janela, última aba e confirmação de fechamento da janela.
- [ ] Montar cabeçalho e menu conforme as referências.
- [ ] Implementar tema sistema/claro/escuro, zoom com limites e tela cheia.
- [ ] Ligar a visão geral ao TabView e validar miniaturas, busca e ações de aba.
- [ ] Criar as preferências essenciais, incluindo fonte, tamanho, tema, histórico e comportamento de fechamento.
- [ ] Persistir preferências com GSettings e fornecer pt-BR/inglês.
- [ ] Centralizar ações e atalhos no GTK e atualizar a ajuda de teclado.
- [ ] Aplicar o ícone oficial às superfícies do aplicativo e à instalação.

**Aceite.** As quatro telas de referência têm correspondentes funcionais. Três abas executam processos independentes, a visão geral seleciona as mesmas abas e preferências sobrevivem ao reinício.

### Marco 4 — Imagens como entrada para agentes

- [ ] Criar ação para escolher imagem e ação específica para colar imagem do clipboard.
- [ ] Aceitar arquivos arrastados sobre o terminal, distinguindo esse gesto da reorganização de abas.
- [ ] Começar por PNG e JPEG, validando conteúdo, dimensões e limites dos modelos/CLIs usados.
- [ ] Guardar imagens do clipboard em cache privado por sessão, com nomes únicos e escrita concluída antes de encaminhar o arquivo.
- [ ] Preservar arquivos originais e texto já digitado no prompt.
- [ ] Permitir remover anexo preparado e copiar seu caminho.
- [ ] Adotar escolha explícita do perfil de agente na aba na primeira versão.
- [ ] Verificar a forma de anexação suportada por cada versão local do CLI.
- [ ] Aproveitar clipboard ou anexo por caminho quando o CLI oferecer esse recurso.
- [ ] Criar adaptações específicas conforme necessário, mantendo estados de arquivo preparado, encaminhado e recebido pelo agente.
- [ ] Encaminhar a ação sem submeter o prompt automaticamente.
- [ ] Preservar nomes com espaços, aspas e caracteres especiais, usando argv para processos e tratando os caminhos de acordo com o destino.
- [ ] Definir retenção do cache para leituras tardias e retomada de sessões, com limpeza explícita de anexos antigos.

| Agente | Versão local | Método de anexação | Modelo com visão | Validação visual |
| --- | --- | --- | --- | --- |
| Codex | A levantar | A verificar | A verificar | Pendente |
| Claude Code | A levantar | A verificar | A verificar | Pendente |
| Hermes | A levantar | A verificar | A verificar | Pendente |
| Gemini | A levantar | A verificar | A verificar | Pendente |

**Aceite.** Copiar uma captura ou arrastar uma imagem, anexar e pedir ao agente uma descrição de texto, cores e formas que só aparecem na imagem. Registrar a versão e a resposta observada. Colar um caminho no prompt será avaliado separadamente do recebimento multimodal.

Usar sessões locais como alvo inicial. Acesso ao arquivo, instalação do CLI, autenticação e modelo com visão são pré-requisitos para validar cada integração. O andamento dos agentes disponíveis pode continuar enquanto os demais aguardam esses pré-requisitos.

### Marco 5 — Recursos de produtividade

- [ ] Transferir abas entre janelas mantendo a sessão e ajustando seus sinais e proprietário.
- [ ] Oferecer busca no histórico e navegação entre resultados.
- [ ] Abrir URLs e hyperlinks do terminal, incluindo regras personalizadas com validação.
- [ ] Salvar ordem das abas, diretórios conhecidos, títulos, geometria e aba selecionada.
- [ ] Versionar o formato, gravar atomicamente e tratar estado parcial ou inválido.
- [ ] Restaurar a organização com novos shells e confirmação explícita antes de executar comandos salvos.
- [ ] Acrescentar perfis de terminal e atalhos configuráveis às preferências.
- [ ] Implementar indicação de atividade e campainha em abas fora de foco.
- [ ] Acrescentar integração opt-in com o shell para início/fim de comandos e código de saída.
- [ ] Definir eventos de conclusão por agente quando houver integração suportada.
- [ ] Emitir notificação quando a tarefa marcada terminar fora de foco, com ação para abrir a aba.

**Aceite.** Mover uma aba preserva o processo. Restaurar o workspace recupera a organização. Busca e links funcionam. Notificações correspondem ao evento marcado e selecionam a aba correta.

### Marco 6 — Consolidação

- [ ] Implementar semântica acessível de texto, seleção e foco para o widget próprio, avaliando a API de acessibilidade GTK e os leitores usados.
- [ ] Conferir Wayland e X11 quando disponíveis, múltiplas escalas, IME e idiomas.
- [ ] Exercitar mudança de tema, fonte e zoom com abas abertas.
- [ ] Conferir estabilidade de snapshots das miniaturas e do scrollback.
- [ ] Medir inicialização, memória, latência de interação e saída extensa em 1, 5 e 10 abas.
- [ ] Avaliar uso de CPU/GPU em foco e em segundo plano.
- [ ] Ajustar renderer com base nas medições, incluindo caches e regiões redesenhadas.
- [ ] Revisar vazamentos, callbacks após fechamento, processos órfãos e falhas de I/O.
- [ ] Completar a comparação das quatro referências e conferir ícone no desktop.
- [ ] Preparar instalação nativa por usuário e instruções de build comprovadas.

**Aceite.** O aplicativo passa pelos cenários de uso diário e fechamento, tem resultados de medição registrados e suas superfícies principais são acessíveis e utilizáveis por teclado.

## 7. Entregas e critérios de conclusão

| Entrega | Inclui | Critério principal |
| --- | --- | --- |
| Prova técnica | Marcos 0 e 1 | Shell funcional e renderização GTK usando libghostty-vt |
| Alpha utilizável | Marcos 2 e 3 | Terminal interativo, abas e interface das referências |
| MVP com agentes | Marco 4 | Anexação validada em Claude Code e Codex, os agentes do MVP |
| Incremento de produtividade | Marco 5 | Sessões organizadas, busca, links e notificações |
| Versão para uso diário | Marco 6 | Estabilidade, acessibilidade e medições de desempenho |

É possível disponibilizar um candidato do MVP com os agentes já validados, apresentando explicitamente o estado das demais integrações. A compatibilidade completa fica concluída após validar Hermes e Gemini, que seguem fora da validação do MVP.

## 8. Estrutura proposta

```text
termac/
  meson.build
  src/
    meson.build
    main.c
    termac-application.c/.h
    termac-window.c/.h
    termac-window.ui
    termac-terminal.c/.h
    termac-session.c/.h
    termac-renderer.c/.h
    termac-attachments.c/.h
    termac-agent.c/.h
    termac-preferences.c/.h
    termac-preferences.ui
    termac-workspace.c/.h
    termac.gresource.xml
  data/
    thiago.aciole.Termac.gschema.xml
    thiago.aciole.Termac.desktop.in
    icons/hicolor/scalable/apps/thiago.aciole.Termac.svg
  po/
    pt_BR.po
  subprojects/
    integração da revisão fixada de libghostty-vt
  PLANO_IMPLEMENTACAO.md
```

Usar GActions para ações e GSettings para preferências. Manter a maior parte da interface em GtkBuilder XML. Criar módulos e arquivos conforme suas entregas entrarem em execução.

## 9. Primeira hora e estimativas

### Primeira sessão de implementação

- Primeiros 15 minutos para verificar ferramentas, fixar a revisão e confirmar o procedimento de build da biblioteca.
- Próximos 20 minutos para ligar a API C à base e criar um estado de terminal.
- Restante da hora para iniciar a conexão GTK/PTY e documentar os resultados e eventuais impedimentos.

A primeira hora termina com um estado técnico verificável e uma estimativa revisada. A execução de um shell na janela depende do progresso da integração.

### Estimativa preliminar de esforço

| Etapa | Faixa inicial |
| --- | --- |
| Dependência, ligação C e primeira tela com shell | 1 a 3 dias |
| Renderer, entrada, seleção e TUI utilizáveis | 3 a 7 dias |
| Abas, menu, visão geral e preferências essenciais | 2 a 4 dias |
| Anexação comum e validação de Claude Code e Codex | 2 a 5 dias |
| Persistência, busca, links e notificações | 3 a 6 dias |
| Acessibilidade, estabilidade e otimização | 4 a 10 dias |

Essas faixas são hipóteses de planejamento, para trabalho concentrado com C e GTK. Revisar após a prova técnica. Renderer, Unicode, acessibilidade e interação com TUIs são os maiores fatores de variação.

## 10. Validação planejada

- Compilação limpa e execução pelo terminal e pelo GNOME Builder.
- Shell com prompts, cores, acentos, emoji e texto largo.
- Editor em tela cheia, pager e CLIs dos agentes.
- Redimensionamento durante saída, entrada composta e colagem multilinha.
- Escritas grandes, fechamento durante I/O e encerramento de shell.
- Seleção, copiar/colar e rolagem consistente entre telas normal e alternativa.
- Abas independentes, visão geral, preferências e restauração de organização.
- Anexação real de imagem por agente, com conteúdo visual confirmado.
- Comparação visual das quatro telas e do ícone.

Esta seção descreve verificações da implementação futura. Os comandos exatos de build serão registrados após a integração da dependência.

## 11. Histórico de progresso — 09/11/2026

### Marco 0

- [x] Revisão Ghostty fixada em `f64f4aca2c29b554d111b36c3d946a9bddd159ff`, a mesma usada pelo Ghostling oficial consultado.
- [x] Essa revisão declara `minimum_zig_version = "0.16.0"`; a integração exige Zig 0.16.0 e o manifesto Builder fixa o arquivo oficial com SHA-256.
- [x] Conferida a API C de `ghostty_terminal_new`, `ghostty_terminal_vt_write`, `ghostty_terminal_resize` e render state. O Ghostling usa Raylib; o Termac usa GTK/Pango no desenho.
- [x] Meson busca Ghostty pelo wrap na revisão fixa e compila `libghostty-vt.a` no prefixo do build.
- [x] ID `thiago.aciole.Termac`, schema, desktop file e ícone fornecido padronizados.
- [x] Build nativo e inicialização GTK/libghostty-vt observados. Feed VT e renderização de prompt, cor ANSI, acento e ideograma conferidos na janela.
- [ ] A execução do build dentro da interface GNOME Builder ainda não foi observada nesta sessão.

### Marco 1

- [x] PTY com shell do usuário criado; leitura via fonte Unix GLib não bloqueante e limitada por iteração, com fila de escrita parcial.
- [x] Render-state mostra prompt colorido, cursor, cor ANSI, `café` e `漢字` sem perda visual.
- [x] Redimensionamento sincroniza libghostty-vt e PTY: observado de 80×29 para 40×12 via `TIOCGWINSZ`.
- [x] Inicialização no backend Wayland confirmada com `GDK_BACKEND=wayland`; o processo conectou a `/run/user/1000/wayland-0` e iniciou o shell filho.
- [ ] Entrada real pelo teclado, sessão TUI, saída extensa e respostas às consultas de dispositivo ainda precisam de validação manual.
- [ ] Falhas de inicialização ainda não têm mensagem com ação de recuperação.

### Marco 3 — primeira fatia implementada

- Código adicionado para abas independentes com `AdwTabView`/`AdwTabBar`, botão de nova aba, fechar aba e fechar a janela ao fechar a última.
- `AdwTabOverview` foi ligado ao mesmo `AdwTabView`, com busca e criação de aba habilitadas; o botão dedicado abre a visão geral.
- O menu declara ações de nova aba, visão geral, fechar aba, preferências, atalhos e Sobre. Atalhos próprios para nova/fechar aba estão registrados.
- Preferências incluem tema Sistema/Claro/Escuro e tamanho de fonte de 8–32 pt, persistidos por GSettings; tamanho da fonte é aplicado às sessões abertas.
- O schema é compilado no build para que execução pelo Meson/GNOME Builder encontre as preferências antes de instalação.
- Compilação passou e o aplicativo permaneceu aberto no Wayland sem avisos fatais, com shell filho ativo.
- [ ] Validar manualmente criação, seleção, fechamento e independência de três sessões, busca/miniaturas no overview, atalhos e persistência após reinício.
- [ ] Conferir visualmente as quatro telas de referência; ainda não foi possível confirmar miniaturas ou o layout na janela desta sessão.

### Ajuste de composição visual — 09/11/2026

- A janela foi reorganizada em duas faixas como na referência: cabeçalho com nova aba à esquerda, título do shell no centro, overview/menu e controles de janela à direita; barra de abas em faixa própria abaixo.
- O título da aba acompanha OSC 0/2 pela callback pública `GHOSTTY_TERMINAL_OPT_TITLE_CHANGED`. Cada shell inicia em `~`.
- O menu tem botões de tema, controles de zoom, nova aba/janela, overview, tela cheia, preferências, atalhos e Sobre. Os atalhos listados também foram registrados no GTK e acrescentados à ajuda.
- Preferências agora mostram cartões para 12 paletas com prévia e amostras de cor. A escolha fica em GSettings e atualiza os terminais abertos via opções de cor da API C.
- O tamanho inicial foi aproximado para 800×650, conforme a proporção da primeira referência.
- Build passou. Abertura Wayland com `G_DEBUG=fatal-warnings` e acionamento da janela de preferências via `gapplication action thiago.aciole.Termac preferences` não produziram avisos; processo permaneceu ativo.
- A interface gráfica não foi exposta pela ferramenta de controle visual nesta sessão; espaçamento final, miniaturas e comparação visual continuam sem conferência direta.
- Registro superado: chegou-se a desativar o `autohide` para mostrar a barra com uma aba. O usuário esclareceu que a faixa deve ficar oculta nessa situação; o código atual reativa `autohide` e remove a altura mínima.

### Entrada de teclado — 09/11/2026

- As teclas especiais agora passam pelo encoder C de libghostty-vt, com atualização dos modos a partir do estado atual do terminal antes de cada evento. Isso atende modo de cursor aplicativo, Kitty keyboard protocol, teclas modificadas e sequências de função conforme suportados pela revisão fixada.
- O mapa cobre Enter (incluindo teclado numérico), Backspace, Tab, Escape, setas, Home/End, Insert/Delete, Page Up/Down e F1–F12; modificadores Shift/Ctrl/Alt/Super são encaminhados.
- A composição de texto continua usando GtkIMContext. Ctrl+A–Z preserva o envio de códigos de controle já existente.
- A checagem sintática C com os headers GTK e Ghostty disponíveis passou. A compilação Meson completa não foi possível neste ambiente porque Zig 0.16.0 não está instalado; o diretório `build/` existente também referencia a antiga localização `/home/thiago/Projects/termac`.
- [ ] Validar fisicamente setas, teclas modificadas e editor/TUI em sessão gráfica; a checagem sintática não comprova o comportamento em execução.

Comandos executados para a prova local:

```sh
PATH=/tmp/termac-tools/zig-x86_64-linux-0.16.0:$PATH meson compile -C build
GDK_BACKEND=x11 G_DEBUG=fatal-warnings build/src/termac
GDK_BACKEND=wayland G_DEBUG=fatal-warnings build/src/termac
```

O build e a abertura da janela passaram. O smoke visual do prompt, sequência ANSI, acentos e caracteres largos foi feito no X11. Em Wayland, `GDK_BACKEND=wayland` iniciou sem avisos fatais, criou o shell filho e conectou ao socket `/run/user/1000/wayland-0`; o método de captura do GNOME Shell foi negado pelo ambiente, então não há captura visual Wayland desta sessão. A tentativa automatizada de entrada por teclado ocorreu no X11 e não validou o caminho de entrada; digitação física, inclusive em Wayland, permanece pendente.
