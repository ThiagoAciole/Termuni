# Termac — referência visual e comportamento

Atualizado em 09/11/2026. As referências originais estão em `/home/thiago/Documentos/termac/screenshoot/` e os ícones fornecidos em `/home/thiago/Documentos/termac/icons/`. O estado e as verificações de cada marco ficam em [PLANO_IMPLEMENTACAO.md](PLANO_IMPLEMENTACAO.md).

## Telas de referência

| Arquivo | Elementos principais |
| --- | --- |
| `Frame 3.png` | Janela escura, título central com caminho, controles no cabeçalho, abas e terminal. |
| `Frame 3 2.png` | Menu com tema sistema/claro/escuro, zoom, nova aba e janela, visão geral, tela cheia, preferências, atalhos e Sobre. |
| `Frame 3 3.png` | Visão geral com busca, quantidade de abas, miniaturas, fechamento e botão Nova aba. |
| `Frame 3 4.png` | Preferências organizadas por áreas; a imagem é alvo de evolução, não comprovação de que todas as opções existam. |

As imagens adicionais fornecidas na conversa detalham a grade de paletas, o menu e o overview. Arquivos temporários de clipboard podem não sobreviver entre sessões; use as imagens originais acima como base persistente.

## Regras acordadas

- Com uma única sessão, ocultar a faixa de abas inteira; com duas ou mais, mostrá-la abaixo do cabeçalho. O botão de nova aba continua acessível no cabeçalho.
- Manter título da sessão e caminho no centro; mostrar controles de nova aba, visão geral, menu e controles da janela seguindo as referências.
- Manter espaço entre o conteúdo do terminal e o cabeçalho e afastar o prompt da borda esquerda. Desde o ajuste de 09/11/2026 o desenho aplica 12 unidades de recuo superior e 8 à esquerda, sem margem extra entre a faixa de abas e o terminal, e a barra de busca fica embaixo, como no Ptyxis. Conferir visualmente em diferentes escalas.
- Usar a janela nativa do GNOME e respeitar tema sistema, claro e escuro. As paletas alteram as cores do terminal independentemente da estrutura do cabeçalho.
- Cada aba representa uma sessão independente; o overview deve mostrar as mesmas abas, permitindo selecionar, fechar e criar. Validar miniaturas e pesquisa antes de considerar o fluxo pronto.
- Preferências devem seguir a organização da referência e apresentar somente opções com comportamento implementado. Atualmente existem controles de tema, tamanho e 12 paletas.

## Identidade e recursos

- `icons/icon.svg` é a arte principal; a cópia do projeto é [thiago.aciole.Termac.svg](data/icons/hicolor/scalable/apps/thiago.aciole.Termac.svg).
- `icons/icon -dev.svg` é a arte de desenvolvimento; a cópia do projeto é [thiago.aciole.Termac.Devel.svg](data/icons/hicolor/scalable/apps/thiago.aciole.Termac.Devel.svg).
- As cópias preservam a arte original. Validar a apresentação no lançador, alternador de janelas e Sobre nos dois perfis; o nome correto do arquivo no build não comprova a apresentação visual no desktop.
- [src/termac-window.ui](src/termac-window.ui) define a estrutura principal; [src/termac-window.c](src/termac-window.c) constrói o menu e ações; [src/termac.css](src/termac.css) contém os estilos dos cartões de paleta. O terminal é desenhado em [src/termac-terminal.c](src/termac-terminal.c).

Ao alterar a interface, compare estados com uma e várias abas, overview e preferências em tamanho lógico comparável às imagens. Registre o que foi observado e o que ainda depende de inspeção visual.

## Referência de implementação: Termac antigo

A cópia local `/home/thiago/Downloads/termac.old` é um fork do Ptyxis, conforme seu `README.md` e `docs/REBRANDING.md`. A análise de 09/11/2026 foi feita pela leitura do código; a cópia não contém `.git` e não foi executada nessa análise. Ela usa VTE e CMake. O Termac atual continua usando libghostty-vt e Meson.

Pontos úteis para refinar a aparência:

- `src/termac-window.ui` organiza `AdwTabOverview`, cabeçalho, barra de abas e `AdwTabView`. O título usa `AdwWindowTitle` com bindings para título e subtítulo da aba ativa. O botão do overview acompanha o estado aberto da visão geral.
- Em `src/termac-window.c`, `update_visible_and_maybe_close()` mostra a barra somente quando há mais de uma página. O XML define `autohide=false`, mas a visibilidade é controlada manualmente pelo número de abas; copiar somente o XML mudaria o comportamento.
- `src/style.css` define no VTE `padding: 7px 6px 3px 6px` e margem inferior de 1 px. Esses valores descrevem o widget VTE antigo, não uma medida diretamente equivalente ao desenho Cairo/Pango atual. O ajuste de 16 no topo e 8 à esquerda do Termac atual já foi mostrado ao usuário; comparar os resultados antes de mudá-lo.
- O menu antigo usa um modelo `GMenu` no XML e `GtkPopoverMenu`, com seções para tema, zoom e ações. `src/termac-theme-selector.ui` e `src/style.css` definem três círculos de no mínimo 44×44, espaçamento de 12 px, margem de 9 px, escolha marcada e a amostra dividida para o tema do sistema. O zoom é ligado ao estado da aba ativa e inclui uma ação para restaurar 100%.
- As preferências em `src/termac-preferences-window.ui` usam páginas de aparência, comportamento, atalhos e perfis. A grade de paletas é um `GtkFlowBox` homogêneo com até três colunas e espaçamento de 9 px; `src/termac-palette-preview.ui` e `src/style.css` descrevem cartões com 12 px de padding, raio de 12 px, indicador de seleção e seis amostras expansíveis de no mínimo 20×16 px, separadas por 3 px. O app atual usa `GtkGrid` de três colunas fixas, cartões mínimos de 175×90 e espaçamento de 12 px; a adaptação a janelas estreitas precisa ser avaliada.

Os arquivos antigos preservam avisos de autoria e GPL-3.0-or-later. Ao adaptar trechos, conservar as atribuições e a licença aplicáveis. Não transportar chamadas específicas de VTE para o motor Ghostty sem redesenhar e validar o comportamento.

A comparação P3 de 13/11/2026 está em [docs/checklists/visual-review.md](docs/checklists/visual-review.md), com capturas Xvfb inspecionadas, divergências dos mockups e verificações GNOME/Wayland ainda pendentes.
