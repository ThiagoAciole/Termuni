# Aceite adicional: pacote, Orca e acesso ao desktop

Continuação de `3ccef25`, em 13/11/2026. Esta rodada não altera o runtime do terminal.

## Pacote instalável e atualização — passou em instalação isolada

Gerado `build/termac-0.1.0-x86_64.flatpak` a partir de `build-flatpak-default`: 5.545.624 bytes, SHA-256 `734924b5f00b224825863a95043eff44cf4bce28a10520c9111776f6ce2f8f81`.

O bundle foi instalado com `FLATPAK_USER_DIR` apontando para `.superpowers/sdd/desktop-acceptance/flatpak-user`. ID `thiago.aciole.Termac`, runtime GNOME 50 e schema do pacote conferidos. A instalação pessoal não foi atualizada.

Pelo `gsettings` do pacote instalado, a fonte foi configurada para 17 com backend keyfile e diretório de configuração privados. Uma segunda exportação/deployment atualizou o commit de `d79160f0…` para `26bcad95…`; a leitura após a atualização manteve **uint32 17**. É uma transação real de instalação/atualização Flatpak, com o mesmo payload da aplicação e novo commit de exportação; não representa migração entre versões diferentes do código. Restauração de sessões entre processos tem a evidência nativa anterior, não foi inferida desta preferência.

[Recibo e checksum](evidence/2026-11-13-package-install.json). Repositório e instalação de teste foram preservados na pasta ignorada de trabalho. Não houve publicação de release ou alteração de remoto Git.

## Orca — leitura e geração passaram; áudio/navegação pendentes

`tests/check-accessible-atspi.py --orca` agora consulta o AT-SPI real por meio de `AXText`, script manager e speech presenter do **Orca 51.0** instalado. O gerador retornou exatamente `ATSPITERM 日本 é 👩‍💻`; leitura do texto e caret pelo Orca também passaram. O cenário existente conferiu seleção e eventos delete/insert/caret/selection.

[Recibo Orca](evidence/2026-11-13-orca-reader.json). Isto verifica bibliotecas reais de leitura/geração, sem simular respostas, mas não inicia áudio nem comprova navegação contínua pelo leitor no desktop.

Foi corrigida uma limitação do runner: ele agora **sempre inicia Xvfb próprio**, mesmo se herdar `DISPLAY` do desktop. Antes só criava display quando a variável estava ausente. Repetição com e sem `--orca` passou em `:110`, com buses privados. Nenhuma configuração pessoal do Orca foi escrita.

```sh
/usr/bin/python3 tests/check-accessible-atspi.py --orca \
  --dbus-daemon /tmp/termac-stable-tools/dbus/usr/bin/dbus-daemon
```

O caminho de dbus-daemon é a extração local usada neste host; em um ambiente com o executável no PATH, omitir a opção. `--orca` é opt-in e foi verificado com a API instalada da versão 51.0; a CI comum não exige Orca.

## Controle nativo — tentativa realizada, aceite não concluído

`agent-cu` está instalado e seu check de permissões respondeu granted. Contudo, `apps` retornou lista vazia e `snapshot --app Builder` respondeu application_not_found, mesmo com o processo Builder registrado e 13 filhos reportados diretamente pelo registry AT-SPI. A captura também falhou por ausência dos utilitários X11. O backend Linux publicado usa xdotool para entrada e import/scrot para captura; não é um controlador Wayland nativo.

Foi possível iniciar uma instância própria do Builder pelo fallback de lançamento do CLI, mas não obter uma árvore verificável para operar a IDE. Portanto não foram enviados cliques/teclas sem alvo observado e não se declarou V01 concluído. A instância própria foi sinalizada para encerrar; o estado anterior de acessibilidade (desligado) foi restaurado. Instâncias Termac de origem não identificada foram preservadas.

Permanecem: compilar/executar pela interface do Builder; IME/gestos Wayland; escala fracionária/aprovação visual; entrega/clique das notificações GNOME; áudio e navegação do Orca. Fontes do controlador: [backend Linux](https://github.com/kortix-ai/agent-computer-use/blob/main/cli/crates/agent-computer-use-linux/src/platform.rs), [entrada/captura](https://github.com/kortix-ai/agent-computer-use/blob/main/cli/crates/agent-computer-use-linux/src/input.rs).

## CI remota — aguardando destino

O checkout não tem remoto e `ThiagoAciole/termac` não foi encontrado. A conta GitHub está autenticada; o workflow e o commit local estão prontos. Foi solicitada a escolha entre criar um repositório privado nesse nome ou usar uma URL existente. Nenhum repositório foi criado nem código enviado sem essa definição.
