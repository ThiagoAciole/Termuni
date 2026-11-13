# P2 — validação real de agentes

Validado em **13/11/2026**. A solicitação do usuário para automatizar a etapa real
foi atendida com os CLIs autenticados rodando no próprio Termac sob Xvfb.
O Computer Use desta sessão não disponibilizou desktop nativo; não se declara
validação dos gestos do compositor Wayland a partir deste teste.

## Versões e contrato dos adaptadores

| CLI | Versão da validação real | Modelo observado | Imagem no help |
| --- | --- | --- | --- |
| Claude Code | 2.1.325 | Sonnet 5.5 | Sem flag anunciada para imagem local. `--file` baixa recursos por file_id. |
| Codex CLI | 0.167.1 | GPT-6.1-Sol medium | `-i, --image <FILE>...` aceita imagens no prompt inicial. |

`claude --help` foi consultado inicialmente em 2.1.323 e novamente após a atualização
automática do próprio CLI para 2.1.325. O Codex permaneceu em 0.167.1; seu aviso de
atualização foi dispensado. Não se inventou nem se acrescentou flag de imagem à ação:
**Abrir agente** inicia a sessão interativa sem anexos iniciais.

Os adaptadores inserem caminho citado e espaço final, sem enviar o prompt.
A seleção em Preferências → Anexos → Agente é persistida; Ctrl+Shift+A abre o agente
na pasta da aba. Ausência do executável e pasta removida geram mensagem antes de
criar uma aba. A detecção usa o mesmo shell/pasta do lançamento, inclusive no host
quando o aplicativo está no Flatpak.

## Resultados observados

As três imagens sintéticas estão em `tests/fixtures/agents/`. O texto enviado pediu
formas, cores e código visível, sem mencionar as respostas nem permitir ferramentas
ou leitura de arquivo como substituto do anexo. Nenhuma chamada de ferramenta foi
mostrada nos diálogos; ambos os modelos responderam a partir das imagens anexadas.

| Cenário | Claude Code 2.1.325 | Codex CLI 0.167.1 |
| --- | --- | --- |
| Abrir pelo fluxo de ação de janela | Passou: CLI real em nova aba | Passou: CLI real em nova aba |
| PNG, nome com espaço/apóstrofo/acento | Quadrado vermelho + círculo azul; **K7R4** | Quadrado vermelho + círculo azul; **K7R4** |
| JPEG | Triângulo amarelo + retângulo roxo; **M2X9** | Triângulo amarelo + retângulo roxo; **M2X9** |
| Clipboard somente imagem, tecla Ctrl+V | Triângulo verde + retângulo laranja; **V8C6** | Triângulo verde + retângulo laranja; **V8C6** |
| Indicador de anexo | `[Image #1]`, `[Image #2]`, `[Image #3]` | `[Image #1]` em cada prompt |
| Arquivo de 20 MiB + 1 byte | Toast **Images must be up to 20 MiB**, sem anexo novo | Mesmo erro, sem anexo novo |
| PNG de 8001×1 px | Toast **The image is larger than 8000 px in width or height** | Mesmo erro |
| CLI removido após seleção | Teste isolado de PATH + toast/contagem de abas passou | API de disponibilidade e fallback agnósticos ao agente; seleção preservada |
| Shell simples, nenhum CLI no PATH | Captura pelo PTY igual ao formato do adaptador | Captura pelo PTY igual ao formato do adaptador |

**6/6 recebimentos multimodais reais confirmados** (PNG, JPEG e clipboard nos dois
CLIs), incluindo a identificação de códigos presentes apenas nos pixels.
As janelas foram encerradas pelo driver de teste; nenhuma sessão do usuário foi reiniciada.

Para bytes, o limite é **até** 20 MiB: ultrapassá-lo causa erro; não se deve rejeitar
uma imagem apenas por estar imediatamente abaixo dele. Não é necessário desinstalar
CLIs pessoais para testar ausência: use PATH controlado como nos testes automatizados.

## Como repetir sem interferir no desktop

O executável de teste usa display Xvfb próprio, XDG isolado e contas já autenticadas.
O driver é opt-in e fica **ignorado por padrão** na suíte comum. Não inicia chamadas
a modelos sem `TERMAC_REAL_AGENT` e `TERMAC_AGENT_CONTROL` explícitos.

```sh
TERMAC_REAL_AGENT=claude TERMAC_AGENT_CONTROL=/tmp/termac-qa-control \
  GSETTINGS_BACKEND=memory GDK_BACKEND=x11 \
  xvfb-run -a build/tests/test-ui -p /ui/real-agent-driver
```

Use `codex` para o segundo adaptador. Em outro terminal:

```sh
python3 tools/agent-qa-control.py /tmp/termac-qa-control snapshot
python3 tools/agent-qa-control.py /tmp/termac-qa-control type /tmp/prompt.txt
python3 tools/agent-qa-control.py /tmp/termac-qa-control drop "tests/fixtures/agents/ação PNG's image.png"
python3 tools/agent-qa-control.py /tmp/termac-qa-control key Return
python3 tools/agent-qa-control.py /tmp/termac-qa-control clipboard tests/fixtures/agents/clipboard.png
python3 tools/agent-qa-control.py /tmp/termac-qa-control key Ctrl+v
python3 tools/agent-qa-control.py /tmp/termac-qa-control quit
```

O arquivo `.screen.txt` traz a grade VT real e o `.ack` aponta para o PNG renderizado
pelo GTK. Cada ação publica nova observação. O timeout sem `quit` é falha, e o driver
fecha apenas a própria janela. A operação `drop` usa o mesmo caminho de importação
que o drop GTK; Ctrl+V é enviado como evento de teclado à janela de teste.

## Limite da evidência

Os resultados acima comprovam o fluxo funcional em **X11/Xvfb**, com conta e rede reais.
Gestos físicos de arrastar do gerenciador de arquivos, clipboard do compositor e
nitidez em Wayland continuam na checklist GNOME do P1. Não foram inferidos destes testes.
`wl-paste`/`xclip` existem no host; o pipeline de Ctrl+V testado aqui usa o clipboard
GDK, armazena a textura e a entrega ao CLI pelo adaptador.
