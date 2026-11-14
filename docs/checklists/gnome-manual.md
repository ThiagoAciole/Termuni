# Verificação GNOME do P1

Atualizado em 13/11/2026. Resultados automatizados ficam em `PLANO_IMPLEMENTACAO.md`.
As células vazias abaixo representam verificações manuais **não realizadas**.
Não reinicie uma sessão com trabalho: informe antes que o shell e suas abas serão encerrados.

## GNOME Builder — não verificado

| Passo | Resultado / ambiente / data |
| --- | --- |
| Abrir `/home/thiago/Projects/termac` no GNOME Builder | |
| Selecionar `thiago.aciole.termac.json`, GNOME 50, perfil development | |
| Compilar e executar pela IDE | |
| Confirmar prompt do shell pessoal e executar `echo ok` | |
| Conferir ausência de GLib-CRITICAL no console | |
| Abrir Preferências, alterar fonte e reiniciar: preferências persistem | |

Reprodução CLI (SDK/Platform 50 devem estar instalados):

```sh
flatpak-builder --force-clean --repo=repo build-flatpak thiago.aciole.termac.json
flatpak install --user ./repo thiago.aciole.Termac.Devel
flatpak run thiago.aciole.Termac.Devel
```

Neste host, SDK/Platform estão na instalação system. `--user --install-deps-from=flathub`
na compilação procura outro SDK na instalação user, que não tem remoto flathub.
O módulo `dir` copia a árvore atual, inclusive alterações sem commit; os diretórios
locais de build e de ferramentas de agentes são excluídos dessa cópia.

Teste automatizado adicional do PTY host (após build com `--keep-build-dirs`):

```sh
python3 tools/test-flatpak-host.py CAMINHO_DO_APP_BUILD CAMINHO_DO_MODULO_TERMAC
```

Use o caminho exato de módulo impresso pelo builder (`Building module termac in ...`);
execuções sucessivas podem gerar `termac`, `termac-1`, `termac-2`. Não reutilize uma
cópia antiga. O driver exige que o teste realmente rode e verifica o PID pelo host.

## Shell do host e imagens

Escolha do usuário: `flatpak-spawn --host --watch-bus`. `script` (util-linux no host)
cria o PTY com controle de tarefas; o shell vem de `$SHELL` do **host**.
`org.freedesktop.Flatpak=talk` permite executar comandos fora do sandbox intencionalmente.
Não há acesso geral à home nem rede no processo gráfico. Os comandos no host têm
os acessos normais do usuário, inclusive rede e arquivos pessoais.

A confirmação de fechamento é conservadora no Flatpak: uma sessão host viva é
considerada ocupada. Não há detecção exata busy/idle por `tcgetpgrp` entre namespaces.
Sem OSC 7 do shell, o diretório salvo permanece o inicial da aba. Pastas restauradas
são verificadas no host; pasta removida volta para `$HOME`.

| Passo | Resultado / ambiente / data |
| --- | --- |
| `echo $SHELL; pwd; which git claude codex` no shell da janela Flatpak | |
| Abrir aba em pasta pessoal restaurada; confirmar `pwd` | |
| `sleep 30`, Ctrl+C duas vezes: voltar ao prompt | |
| Fechar aba com sleep: confirmar ausência desse PID no host | |
| Colar imagem em Claude Code: confirmar “Image attached to the prompt” e anexo | |

O runtime não contém `wl-paste`/`xclip`; a opção escolhida executa Claude Code no host,
onde ambos foram encontrados. Portanto não se empacota wl-clipboard no sandbox.
Recebimento multimodal funcional foi repetido no Flatpak/Xvfb, 6/6, conforme a checklist de agentes. Gestos físicos do compositor permanecem separados; localizar o executável não comprova anexo.

## Ícones — aparência não verificada

| Passo | Resultado / ambiente / data |
| --- | --- |
| Iniciar pelo lançador e conferir ícone no dock/alternador de janelas | |
| Conferir perfil development e perfil default | |
| Conferir ícone no Sobre e em tema claro/escuro | |

As fontes SVG originais ficam intactas. A instalação gera um canvas quadrado com
margem transparente: o Flatpak rejeitava o canvas original de 250×220.
O projeto ainda não tem homepage pública nem histórico de releases publicado;
o teste AppStream reduz **apenas** `url-homepage-missing` a informação. A validação
pedante sem essa exceção continua apontando a homepage ausente. IDs são preservados.

## Orca — geração verificada; interação e áudio pendentes

| Passo | Resultado / ambiente / data |
| --- | --- |
| Ativar Orca e abrir Termac | |
| Faixa de abas anuncia títulos, seleção e fechamento | |
| Botões Nova aba, Visão geral, menu e busca são falados | |
| Preferências anuncia páginas, linhas, fontes e paletas | |
| Digitação e saída do terminal são anunciadas adequadamente | |

Orca 51 leu o texto e gerou a frase Unicode em teste privado: ver [aceite adicional](../reports/2026-11-13-desktop-acceptance.md). Isso não comprova fala/navegação contínua no desktop. O teste de nomes cobre controles públicos e o papel terminal. Botões privados de
GtkMenuButton e GtkSpinButton pertencem ao controle composto e são excluídos;
controles invisíveis são ignorados. Não demonstra leitura do conteúdo do terminal.

## Escala Wayland — não verificado

Em Configurações GNOME → Monitores, selecionar cada escala e reabrir a janela de teste.
A suíte Xvfb com GDK_SCALE=2 e GDK_DPI_SCALE=1.25 não simula escala fracionária Wayland.

| Escala | Texto nítido | Seleção alinhada | Sem cortes/desfoque | Ambiente / data |
| --- | --- | --- | --- | --- |
| 125% | | | | |
| 150% | | | | |
| 200% | | | | |

## Pacote instalado e atualização isolada

O bundle default foi instalado em FLATPAK_USER_DIR de teste e atualizado para outro commit de exportação, preservando a fonte 17 com backend keyfile privado. [Evidência e limites](../reports/2026-11-13-desktop-acceptance.md). O payload da aplicação é o mesmo; lançador/dock e restauração em atualização entre versões ainda não foram aceitos no desktop.
