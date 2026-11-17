# Janelas auxiliares sem sobreposição preta

Solicitação de 13/11/2026: substituir os diálogos embutidos que cobrem a janela do terminal por janelas separadas, conforme a captura fornecida pelo usuário.

## Mudança

`src/termac-dialog.h` centraliza a apresentação standalone suportada por libadwaita: `adw_dialog_present`/`adw_alert_dialog_choose` com parent de apresentação NULL, configurando depois a janela nativa como modal/transiente da origem. Assim o libadwaita não cria uma sheet nem o backdrop dentro do terminal. Grupos de ações e associação à aplicação são preservados, incluindo atalhos dinâmicos.

Aplicado a fechar aba, fechar janela, sair do aplicativo, renomear aba, Sobre, Atalhos e captura de atalhos. Preferências já era uma janela. O seletor de arquivos continua usando GtkFileDialog/portal.

Cancelar e fechar pelo X preservam os programas ativos. Ao destruir a origem, as auxiliares seguem o caminho normal de fechamento, resolvendo callbacks pendentes como cancelamento e liberando referências. Os handlers da origem são removidos quando cada janela auxiliar fecha, evitando fechar novamente um diálogo já encerrado. A saída do aplicativo também fecha o conteúdo standalone pelo caminho normal antes de destruir suas janelas, liberando builders e escolhas assíncronas.

Na regressão de renomear foi encontrado um erro existente: o botão respondia com ID `close`, mas o callback esperava `rename`. O ID foi alinhado e a resposta de renomear passou a ser a padrão para a entrada.

## Verificação

- Build nativo passou; build no SDK GNOME 50 (GTK 4.22/libadwaita 1.9) também passou. `/ui/dialog-windows` também passou dentro do sandbox com shell do host.
- Suíte completa: **13/13**, UI normal 230,37 s e HiDPI 243,68 s (59 casos UI registrados; seis auxiliares opt-in). A regressão de saída foi depois ampliada para incluir Atalhos aberto, com repetição direcionada normal/HiDPI (4/4 casos em cada) e `/ui/quit-confirmation` também passando no SDK/Flatpak.
- `/ui/dialog-windows`: confirma roots nativos distintos e ausência de `visible-dialog` no parent; modal/transiente corretos; X e Cancelar sem interromper o shell; renomear aplicado; Sobre/Atalhos separados; captura e confirmação liberadas ao destruir sua origem.
- `/ui/quit-confirmation`, `/ui/shortcut-editor` e `/ui/shortcut-conflict` passaram nas execuções direcionadas. Os testes foram adaptados para localizar as novas janelas, preservando as verificações de conflito/cancelamento.
- `/ui/visual-screens` passou e produziu capturas de Sobre/Atalhos em suas próprias janelas. As imagens foram abertas: conteúdo e combinações de teclas continuam visíveis.

Capturas locais preservadas em `build/archive/previous-development/dialog-windows-shots/close-window-parent.png`, `close-window-native.png`, e `build/archive/previous-development/dialog-windows-visual/about.png` e `shortcuts.png`. Os snapshots da origem e da confirmação foram inspecionados separadamente, sem a camada preta da apresentação embutida. A regra de UX fica em `docs/design/DESIGN.md`.

Documentação primária: [AdwDialog.present](https://gnome.pages.gitlab.gnome.org/libadwaita/doc/main/method.Dialog.present.html), [AdwAlertDialog.choose](https://gnome.pages.gitlab.gnome.org/libadwaita/doc/main/method.AlertDialog.choose.html). A solução usa o caminho público standalone, sem APIs obsoletas ou CSS sobre estruturas internas do Adwaita.

Nenhuma sessão pessoal foi reiniciada. A mudança está no build do projeto; processos que já estavam abertos continuam executando sua versão anterior até serem reabertos. O aceite físico no compositor Wayland permanece separado dos testes Xvfb.
