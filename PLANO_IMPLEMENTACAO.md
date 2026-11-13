# Plano de implementação do Termac

Atualizado em 09/11/2026 para C, GTK 4, libadwaita e libghostty-vt.

## Estado atual — 09/11/2026

Este quadro é o ponto de retomada. As seções 1–10 preservam o plano e suas condições no momento em que foi escrito; a seção 11 registra observações históricas. Quando divergirem deste quadro ou do código atual, confira a evidência mais recente antes de agir. O detalhamento técnico está em [PDR.Arquiteture.md](PDR.Arquiteture.md) e as referências visuais em [DESIGN.md](DESIGN.md).

| Marco | Situação | Evidência e trabalho pendente |
| --- | --- | --- |
| 0 — dependência e build | Parcial | O wrap fixa Ghostty em `f64f4aca2c29b554d111b36c3d946a9bddd159ff`; Zig `0.16.0` é exigido. Por decisão do usuário, o manifesto do Builder usa GNOME 50 para ampliar compatibilidade. O staging vazio `x86_64-main` foi inicializado com o app ID e as refs corretas; `flatpak build ... /usr/bin/true` confirmou que o erro de staging desapareceu. A compilação e a execução do Termac pela IDE ainda não foram confirmadas. |
| 1 — shell e PTY | Parcial | Shell e prompt renderizado foram observados em Wayland; o histórico abaixo registra provas anteriores de cores, Unicode e resize. Validar entrada real, TUI, carga, respostas VT, tratamento de falhas e encerramento antes de aceitar o marco. |
| 2 — interação | Parcial | Há GtkIMContext, tratamento de teclas e encoder Ghostty no código. Falta validar entrada composta, teclas e TUI; seleção, copiar/colar, scrollback e política de fechamento ainda estão pendentes. |
| 3 — interface | Parcial | Código de abas, overview, menu, ações, tema, tamanho de fonte e paletas existe. A captura mais recente mostrou o prompt praticamente junto ao topo da área; foi adicionado recuo superior de 24 px no desenho. A janela ativa não foi reiniciada para preservar a sessão; falta conferir visualmente o novo binário, além de múltiplas sessões, overview, atalhos e persistência. |
| 4 — imagens para agentes | Planejado | Não há implementação ou validação de anexação real por agente registrada. |
| 5 — produtividade | Planejado | Busca, links, restauração e notificações permanecem no plano. |
| 6 — consolidação | Planejado | Acessibilidade, desempenho, estabilidade, instalação e comparação visual completa permanecem pendentes. |

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
  - Não implementado nesta rodada: perfis de terminal, edição de atalhos pela interface (as ações existem e o schema permite ajustar, mas sem editor), escolha explícita de perfil de agente e adaptadores por CLI (Codex, Claude Code e OpenCode estão instalados, mas o anexo nativo exige flags no momento de iniciar o agente, o que não se injeta num prompt já aberto), eventos de conclusão por agente, realce visual dos resultados da busca, medições de desempenho em 1, 5 e 10 abas, verificação de vazamentos (valgrind não está instalado), instalação por usuário, e acessibilidade além do rótulo e papel do widget.

- Ajuste de menu e anexos em 09/11/2026: removidos do menu principal "Colar imagem" e "Anexos"; Ctrl+Shift+V cola imagem como anexo quando o clipboard tem só imagem, e os anexos passaram a uma aba "Anexos" em Preferências (copiar caminho, remover, limpar tudo). Também saíram do menu "Buscar no terminal" e "Mostrar abas abertas", que já têm botões no cabeçalho; os atalhos Ctrl+Shift+F e Ctrl+Shift+O continuam. Verificação: `meson compile -C build` sem erros e `meson test` com 6 testes passando. Não verificado visualmente; a instância aberta usa o binário anterior.

**Próxima retomada sugerida:** validar manualmente no Wayland a entrada pelo teclado, TUI, seleção e colagem com mouse, busca, anexos, renomear e reordenar abas, e as notificações. Depois implementar perfis, edição de atalhos e a integração por agente. Em paralelo, verificar duas ou mais abas, overview e preferências contra as referências. Para build de distribuição, resolver o schema dependente do diretório de build e validar o fluxo real do Builder.

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
| OpenCode | A levantar | A verificar | A verificar | Pendente |
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
| MVP com agentes | Marco 4 | Anexação validada individualmente nos cinco agentes alvo |
| Incremento de produtividade | Marco 5 | Sessões organizadas, busca, links e notificações |
| Versão para uso diário | Marco 6 | Estabilidade, acessibilidade e medições de desempenho |

É possível disponibilizar um candidato do MVP com os agentes já validados, apresentando explicitamente o estado das demais integrações. A compatibilidade completa fica concluída após validar os cinco alvos.

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
| Anexação comum e validação dos cinco agentes | 2 a 5 dias |
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
