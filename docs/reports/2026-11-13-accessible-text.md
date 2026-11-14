# M04 — conteúdo acessível do terminal

Implementado `GtkAccessibleText` em `TermacTerminal`. O documento acessível é a grade visível, com espaços de células vazias e uma quebra por linha física. Ao rolar, o mesmo contrato expõe o histórico visível; não materializa todo o scrollback em cada consulta. A leitura usa somente `grid_ref`, `grid_ref_graphemes`, `terminal_get` e conversão de coordenadas, sem consumir os dirty flags do renderer.

Offsets contam pontos de código Unicode. CJK ocupa duas células e um caractere; marcas combinantes e sequências ZWJ mantêm seus pontos de código e compartilham a geometria do grafema. A interface fornece fatias, limites de caractere/palavra/frase/linha/parágrafo (Pango para palavra/frase), caret, seleção linear/reversa/retangular e geometria em coordenadas do widget. Linhas e parágrafos correspondem às linhas físicas da grade.

O caret pertence ao programa conectado ao PTY; quando está fora do viewport, o offset reportado é zero. Os setters acessíveis de caret e seleção retornam `FALSE`: não simulam teclas nem fingem editar o texto de um aplicativo em execução. Atributos de estilo não são expostos nesta implementação básica.

As atualizações comparam prefixo/sufixo Unicode e notificam apenas o intervalo alterado. Durante a remoção, GTK ainda lê o conteúdo anterior; a inserção é anunciada depois da troca do snapshot. Alterações de caret e seleção também são anunciadas. O cache pertence ao widget e é liberado no dispose; referências de grid não sobrevivem a consultas.

## Verificação executada

Compilação isolada do worktree contra GTK 4.24 do host e a biblioteca Ghostty já construída com a revisão fixada do projeto:

```sh
python3 /home/thiago/Projects/termac/.superpowers/sdd/stable-release/build-ui.py "$PWD" /tmp/termac-stable-a11y-ui
GSETTINGS_SCHEMA_DIR=/home/thiago/Projects/termac/build/data GDK_BACKEND=x11 xvfb-run -a /tmp/termac-stable-a11y-ui -p /ui/accessible-text
```

O teste passou em escala normal e HiDPI (`GDK_SCALE=2 GDK_DPI_SCALE=1.25`). A regressão existente `/ui/truecolor-unicode-render` também passou. Verifica conteúdo, fatias Unicode/CJK/combining/ZWJ, limites, clamp no fim, caret, seleção linear/reversa/retangular/limpa, setters sem efeito, geometria, texto do scrollback e preservação de atualização pendente no renderer após consulta acessível. Interposição de `gtk_accessible_text_update_contents`, encaminhada à função GTK real, verifica o payload antigo da remoção e o novo da inserção, inclusive offsets Unicode.

Este teste exercita os callbacks e notificações reais de `GtkAccessibleText`; não comprova transporte AT-SPI, fala no Orca, Wayland, GNOME Builder ou SDK Flatpak. Não havia `pyatspi` nem `dbus-run-session` disponíveis no ambiente usado. O aceite manual V06 permanece pendente. A compilação Meson completa e repetição no SDK ficam com a integração desta alteração.

Contrato de notificação conferido na [documentação primária GTK](https://docs.gtk.org/gtk4/method.AccessibleText.update_contents.html).
