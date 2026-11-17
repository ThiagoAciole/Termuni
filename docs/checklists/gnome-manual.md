# GNOME, pacote `.deb` e acessibilidade

Atualizado em 14/11/2026. Resultados observados ficam em `PLANO_IMPLEMENTACAO.md` e nos relatórios datados. Células vazias são verificações manuais ainda não realizadas.

## GNOME Builder nativo — pendente

| Passo | Resultado / ambiente / data |
| --- | --- |
| Abrir `/home/thiago/Projects/termac` como projeto Meson | |
| Configurar um diretório de build novo com `profile=development` | |
| Compilar e executar pela IDE | |
| Confirmar shell local, `pwd` e `echo ok` | |
| Conferir ausência de GLib-CRITICAL no console | |
| Abrir Preferências, alterar fonte e confirmar persistência após reiniciar | |

Não selecione `default` para desenvolver no mesmo diretório de build. O perfil development mantém seu ID, schema e ícone próprios.

## Pacote Debian local

O perfil `default` foi compilado e instalado nesta máquina em 14/11/2026. O registro da operação está em [instalação local](../reports/2026-11-14-local-deb-install.md). A geração do pacote pode ser repetida com:

```sh
python3 tools/build-deb.py
sudo apt install ./dist/thiago.aciole.termac_0.1.0_amd64.deb
```

| Passo | Resultado / ambiente / data |
| --- | --- |
| Conferir `dpkg-query -W thiago.aciole.termac` | Concluído: 0.1.0 amd64 instalado, 14/11/2026 |
| Confirmar `/usr/bin/termac` e abrir pelo menu de aplicativos | Binário conferido; abertura visual pendente |
| Confirmar `Icon=thiago.aciole.Termac` e o ícone em `/usr/share/icons/hicolor/scalable/apps/thiago.aciole.Termac.svg` | Concluído: arquivo pertence ao pacote; 14/11/2026 |
| Verificar no dock e no alternador de janelas | |
| Abrir abas em pastas diferentes e confirmar o shell/CWD local | |
| Executar `sleep 30`, interromper com Ctrl+C e fechar a aba após retornar ao prompt | |
| Abrir um programa longo e conferir a confirmação ao fechar a aba | |
| Colar imagem em Claude Code e Codex quando instalados | |

## Aparência e escalas

| Passo | Resultado / ambiente / data |
| --- | --- |
| Conferir ícone no Sobre e em tema claro/escuro | |
| Conferir tamanho de fonte, bordas e margens no desktop | |
| Conferir 125%, 150% e 200% no compositor Wayland | |

Xvfb com `GDK_SCALE=2` e `GDK_DPI_SCALE=1.25` não simula escalonamento fracionário Wayland.

## Orca

| Passo | Resultado / ambiente / data |
| --- | --- |
| Ativar Orca e abrir Termac | |
| Ouvir a faixa de abas, botões e Preferências | |
| Confirmar leitura de digitação e saída do terminal | |

Orca 51 leu texto e gerou a frase Unicode em teste privado: ver [aceite adicional](../reports/2026-11-13-desktop-acceptance.md). Isso não comprova navegação contínua nem fala no desktop.
