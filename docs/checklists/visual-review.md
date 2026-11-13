# Revisão visual P3 — 13/11/2026

Inspeção das capturas GTK feita pelo agente sob Xvfb/X11, GTK 4.24.1 e libadwaita 1.10.0. As quatro imagens `Frame 3*.png` de `/home/thiago/Documentos/termac/screenshoot/` e os três `assets/tema-*.png` foram abertos para comparação. Isto não equivale à aprovação visual do usuário nem à inspeção do desktop GNOME/Wayland.

Reprodução (diretório temporário, preferências e sessões isoladas pelo teste):

```sh
meson compile -C build
mkdir -p /tmp/termac-p3-shots
TERMAC_UI_SCREENSHOTS=/tmp/termac-p3-shots GSETTINGS_BACKEND=memory \
  GDK_BACKEND=x11 xvfb-run -a build/tests/test-ui
# Apenas as telas principais:
TERMAC_UI_SCREENSHOTS=/tmp/termac-p3-shots GDK_BACKEND=x11 \
  xvfb-run -a build/tests/test-ui -p /ui/visual-screens
```

Os popovers têm superfície nativa própria: `menu-*.png` captura o popover diretamente. Uma captura apenas da janela omitia o menu, por isso essas imagens iniciais foram descartadas. A captura espera a apresentação da janela e o fim da transição do overview; uma tentativa anterior durante a transição não produziu nó de renderização. A ativação normal do aplicativo carrega o CSS antes das capturas.

| Item / referência | Esperado | Observado | OK? |
| --- | --- | --- | --- |
| Cabeçalho, `Frame 3.png` | Overview no início, título e caminho centrais, nova aba/menu/controles no fim | Ordem preservada; busca também aparece no início, conforme a interface atual. Ícones simbólicos do GTK diferem do desenho antigo | Sim para estrutura; aprovação estética no GNOME pendente |
| Recuos, DESIGN.md | 12 unidades acima e 8 à esquerda | Texto afastado do cabeçalho e da borda, em uma e duas abas; constantes do desenho 12/8 | Sim em 1×; confirmar escala fracionária real |
| Menção a 24 px no plano P3 | Resolver divergência com DESIGN.md | Preservadas as medidas atuais 12/8 do DESIGN.md; 24 px é registro histórico | Divergência documentada |
| Faixa de abas | Oculta com uma sessão, visível com duas ou mais | `main-one-tab.png` sem faixa; `main-two-tabs.png` com duas abas. O teste também verifica `tabs-revealed` | Sim |
| Terminal e cromia | Paleta independente da estrutura da janela | `palette-{gnome,dracula,ubuntu,nord,high-contrast}.png` mantém a estrutura e altera as cores; texto legível nas capturas | Sim nas amostras; contraste medido/Orca não verificados |
| Overview, `Frame 3 3.png` | Busca, duas miniaturas, fechar, título e Nova aba | Elementos presentes, duas miniaturas correspondem às sessões e botão inferior visível | Sim; fontes/ícones e proporções diferem da referência |
| Menu, `Frame 3 2.png` | Tema, zoom, ações, Preferências, Atalhos, Sobre | Controles presentes; Open Agent e anexar imagem refletem P2. Busca/overview ficam no cabeçalho, conforme decisões atuais | Sim funcional; composição atual difere do mockup |
| Tema, `assets/tema-claro.png` | Arte clara, seleção e tema claro | `menu-light.png`: mesma arte reduzida, contorno azul na seleção; fundo claro do app | Sim |
| Tema, `assets/tema-escuro.png` | Arte escura e seleção | `menu-dark.png`: arte escura selecionada e app escuro | Sim |
| Tema, `assets/tema-automatico.png` | Arte dividida, seguir sistema | `menu-system.png`: arte dividida selecionada; Xvfb forneceu aparência escura | Arte: sim; seguir preferências reais do GNOME pendente |
| Preferências, `Frame 3 4.png` | Áreas organizadas, somente opções implementadas | Geral/Aparência/Atalhos/Anexos na barra inferior, em vez das abas superiores do mockup. Perfis continuam fora da primeira versão | Diferença de composição registrada; não acrescentados controles fictícios |
| Cabeçalho de Preferências | Um botão de fechar | Captura inicial tinha botão explícito e controle nativo duplicados. Desativados controles nativos deste cabeçalho; novo PNG mostra um só botão | Corrigido; antes em `/tmp/termac-p3-preferences-before.png` |
| Cartões de paleta e espaçamento | Três colunas, seis amostras, seleção identificável | Nove cartões iniciais, doze ao expandir; seis amostras e marca/contorno na seleção; espaçamento de 12 unidades | Sim no tamanho capturado |
| Medidas antigas dos cartões em DESIGN.md | Documento citava mínimo 175×145 | Código e captura atuais usam 175×90; cartões não têm recorte horizontal, página rola verticalmente | Divergência documental; sem alteração arbitrária de altura |
| Geral | Preferências de restauração, histórico, integração, notificação e fechamento | Grupos legíveis e notificação desligada por padrão em `preferences-behavior.png` | Sim |
| Atalhos editáveis | Ações e restaurar padrões visíveis | `preferences-shortcuts.png`: ações e combinações alinhadas; restante acessível por rolagem. Conflito testado pelo caminho real do editor | Sim |
| Anexos | Seletor do agente, estado vazio e limpeza | `preferences-attachments.png`: agente e explicações visíveis, sem sobreposição | Sim |
| Sobre | Identidade Termac, versão, créditos e licença | `about.png`: ícone do perfil de desenvolvimento, nome, 0.1.0, créditos e legal | Sim para este perfil; lançador e perfil default no GNOME pendentes |
| Diálogo de atalhos | Categorias e combinações legíveis | `shortcuts.png`: navegação e teclas legíveis, conteúdo longo com rolagem | Sim para padrões; adaptação do diálogo a atalhos personalizados não faz parte desta mudança |
| Busca | Barra inferior e todas as ocorrências destacadas | `search-highlight.png`: quatro ocorrências de marker (comando + saída), destaque azul translúcido, atual mais forte; sem destaque após fechar | Sim; CJK/emoji/acentos cobertos nos testes do motor |
| Bordas, fonte e escala | Integração nativa no GNOME | Xvfb não tem o compositor, tema de desktop e escala fracionária da sessão real; bordas retangulares e fontes diferem dos mockups | Precisa de sessão real |

Capturas principais em `/tmp/termac-p3-shots/`:

- `main-one-tab.png`, `main-two-tabs.png`, `tab-overview.png`;
- `menu-light.png`, `menu-dark.png`, `menu-system.png`;
- `palette-gnome.png`, `palette-dracula.png`, `palette-ubuntu.png`, `palette-nord.png`, `palette-high-contrast.png`;
- `preferences-behavior.png`, `preferences-appearance.png`, `preferences-all-palettes.png`, `preferences-shortcuts.png`, `preferences-attachments.png`;
- `about.png`, `shortcuts.png`, `search-highlight.png`.

A execução completa também salva as capturas de regressão já existentes (terminal, seleção, IME, anexos, temas e zoom). Esses arquivos são temporários e podem ser regenerados; não foram adicionados binários PNG ao Git.

## Validações reais ainda pendentes

- [ ] Aprovação visual do usuário nas telas, fontes, bordas, ícones, temas e escala fracionária de uma sessão GNOME/Wayland real.
- [ ] Ligar “Notificar quando tarefas terminarem”, executar `sleep 12; echo done`, selecionar outra aba e observar a notificação GNOME. Clicar nela deve selecionar a sessão original.
- [ ] Repetir com a janela fora de foco, com a preferência desligada e com a aba já fechada antes do clique. O último caso tem cobertura automatizada da ação `app.show-session`, mas a entrega/clique do desktop não foi observado.

A política pura cobre as 16 combinações de preferência/foco/elegibilidade. Relatos explícitos da integração Bash mantêm a elegibilidade de comandos curtos do comportamento anterior; a detecção por processo exige a duração mínima existente. Sem conexão de controle nativo do desktop nesta sessão, não se declarou entrega de notificações no GNOME como testada.
