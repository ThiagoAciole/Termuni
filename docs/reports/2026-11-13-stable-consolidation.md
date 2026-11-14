# Consolidação da primeira versão estável

Implementação em `fix/stable-release`, iniciada em `b5465a4`. Este relatório sucede a auditoria de organização; não substitui o aceite físico GNOME/Wayland. Nenhuma sessão pessoal foi reiniciada.

## Correções e arquitetura entregues

- **M01/M12:** testes de carga, interrupção, seleção, anexos e TUIs receberam shells próprios e marcadores que exigem execução real. `seq 1 30000` tem comparação integral das 30.000 linhas/168.894 bytes. `vi` usa nome relativo para evitar o prompt de Enter causado por prefixos temporários longos. Baselines de PID aguardam os reapers assíncronos. Restart de desenvolvimento distingue executável/perfil/checkout e possui regressão própria.
- **M02/M09:** hit testing/sublinhado/abertura compartilham células e grafemas reais do Ghostty. OSC 8 com rótulos funciona, com URI HTTP(S) validada. Regressões incluem CJK, combining, ZWJ, largura legada e protocolos inválidos.
- **M03:** dispose deixa de esperar pelo filho. Child watch assume PID, PTY e ownership da coleta; envia SIGHUP e escala para SIGKILL após 1 s. O widget é liberado sem aguardar esse prazo. O processo da aplicação permanece vivo até a coleta. `app.quit` confirma tarefas ativas, permite cancelar e salva antes de fechar. Um teste entre processos prova a saída com filho que ignora SIGHUP.
- **Bridge:** o teste real encontrou um processo no host sobrevivendo à primeira implementação assíncrona. O PTY agora fica com o reaper durante o encaminhamento do HUP por `flatpak-spawn`; não fecha no dispose do widget. Repetição development/default passou e confirmou a coleta do PID no host. Grupos do host não são sinalizados como PIDs locais do sandbox.
- **M15/V07:** snapshots imutáveis, writer dedicado, coalescência/ordenação e drenagem no shutdown. Criação/substituição usam `CONSISTENT | DURABLE`, modo 0600, diretório 0700. Erro mantém o último arquivo válido, retry acontece no próximo checkpoint/mutação, e v2 permanece intacto. V3 existente não é substituído por importação de v2 obsoleto. Testes cobrem criação, permissões, link simbólico, erro real/injetado, ordem e reinício de executáveis em processos distintos. Não houve simulação de corte físico de energia.
- **M04:** `GtkAccessibleText` fornece texto Unicode, caret, seleção, limites e geometria sem consumir dirty flags do desenho. Cliente AT-SPI real em buses privados leu `ATSPITERM 日本 é 👩‍💻`, caret 9→11, seleção [0,9]→vazia e eventos delete/insert/caret/selection. Aceite com Orca permanece separado. GTK mínimo 4.22 foi declarado, compatível com SDK GNOME 50 e os setters da interface.
- **M10/M14:** conexão encerrada/fila cheia têm feedback traduzido sem repetição por tecla; escrita falha limpa dados não entregáveis. Colagem de 2 MiB continua íntegra. `COLORTERM=truecolor` é exportado nativamente e no bridge, com valor conferido no shell real do host. Motor/desenho RGB, CJK/emoji e btop continuam cobertos.

## Verificação integrada final

| Perfil | Suítes Meson | UI normal | UI HiDPI |
| --- | --- | --- | --- |
| development | 13/13 passaram | 230,81 s | 247,27 s |
| default | 13/13 passaram | 241,38 s | 251,83 s |

São 58 casos registrados por execução UI, seis auxiliares opt-in pulados na suíte comum. AT-SPI, agentes reais e bridge foram executados separadamente como descrito abaixo. Tradução pt-BR: 161 mensagens, zero fuzzy; sintaxe/formatos gettext passaram. [Resumo sem variáveis de ambiente](evidence/2026-11-13-stable-validation.json) preserva nomes/resultados/tempos e hashes dos executáveis. Instalação nativa em DESTDIR passou nos dois perfis. Uma rodada concorrente teve timeout de `dev-tool-isolation`; a repetição isolada passou em 9,95 s, além das execuções finais completas registradas.

## Distribuição e agentes

Development e default possuem build e instalação nativa em DESTDIR isolado. Flatpak GNOME 50 compilou nos dois perfis; no default foram conferidos ID `thiago.aciole.Termac`, schema instalado e **11/11 testes** de util, VT, anexos, settings, agentes/boundary, isolamento de dev, session-store e metadata. O teste de PTY host passou em ambos, incluindo Ctrl+C e coleta do processo após fechar.

Claude Code 2.1.325 e Codex CLI 0.167.1 repetiram **6/6 anexos reais pelo Flatpak/host**: PNG K7R4, JPEG M2X9 e clipboard/Ctrl+V V8C6, com cores/formas correspondentes. O prompt não continha os códigos e proibia ferramentas/leitura de arquivos. Detalhes em [agentes](../checklists/agents-manual.md). A atualização sugerida pelo Codex foi dispensada; CLIs pessoais não foram atualizados.

O manifesto de release usado localmente foi gerado do manifesto development, alterando apenas ID/perfil e caminho da fonte; ficou no diretório ignorado de trabalho. Os builds estão em `build-flatpak` e `build-flatpak-default`; não houve instalação global ou publicação de release. A instalação/atualização no desktop continua no aceite da distribuição.

## CI

[Workflow](../../.github/workflows/ci.yml) para development/default: build nativo Fedora 44, UI normal/HiDPI, traduções, schema/Desktop/AppStream, instalação isolada e AT-SPI privado; jobs Flatpak GNOME 50 geram artifacts. Zig usa a URL/hash do manifesto e Ghostty mantém seu wrap fixado. Logs são preservados em artifacts; contas reais de agentes são opt-in e não exigidas pela CI.

`bash tools/ci-check.sh development` / `default` reproduzem o caminho nativo localmente. YAML, sintaxe Bash/Python e gettext foram verificados. **O workflow ainda não foi executado no GitHub:** não houve publicação/push deste repositório.

Fontes da configuração: [flatpak-github-actions](https://github.com/flatpak/flatpak-github-actions), [imagem oficial GNOME 50](https://github.com/flathub-infra/actions-images), [GTK 4.22 no Fedora 44](https://packages.fedoraproject.org/pkgs/gtk4/gtk4-devel/fedora-44.html).

## Memória e desempenho

Valgrind 3 e 15 ciclos: testes passaram e **22.016 bytes/20 blocos definitivos em ambas as execuções**, sem frames do Termac nas perdas definitivas nem acessos inválidos reportados. Logs em `build-valgrind/valgrind-rounds-{3,15}.log`. O residual é de bibliotecas; o exit code diagnóstico do script, sozinho, não é o critério. Uma referência de ícone por aba já havia sido corrigida no P0.

Benchmark: **9 amostras**, 1/5/10 abas, 3.000 linhas por aba comparadas integralmente, Cairo/Xvfb. [JSON com amostras e ambiente](evidence/2026-11-13-benchmark.json).

| Abas | Mapear janela/abas, mediana | Saída de todas as abas, mediana | 20 buscas, mediana | RSS observado |
| --- | --- | --- | --- | --- |
| 1 | 105,5 ms | 2,40 s | 60,1 ms | 193,2–194,3 MiB |
| 5 | 164,2 ms | 12,57 s | 46,8 ms | 199,1–200,0 MiB |
| 10 | 269,9 ms | 25,96 s | 53,0 ms | 208,5–209,9 MiB |

RSS/CPU incluem somente o frontend. O tempo de shell pronto inclui aproximadamente 700 ms por aba do driver de entrada; saída também inclui o driver. Resize inclui dez pausas de 30 ms. As condições de ociosidade são janela apresentada/outra janela apresentada no Xvfb, sem aceite de foco pelo compositor Wayland. Não comparar estes números a um benchmark de GPU ou latência física. Há espaço para perfilamento do custo de saída/desenho; não foi trocado o renderer nem introduzida otimização sem medição específica.

Uma tentativa anterior concorrente com compilação Zig e suíte completa esgotou a memória/swap e estourou prazos; não é a matriz publicada. Builds/logs temporários próprios foram preservados no disco do projeto, com links nos caminhos antigos, liberando o tmpfs. O benchmark encerra seu próprio grupo de processos em timeout.

## Aceites restantes

GNOME Builder pela IDE; teclado/IME/gestos reais Wayland; escala fracionária e aprovação visual; entrega/clique de notificações GNOME; Orca; instalação/atualização do pacote no desktop; execução remota da CI. O controle de aplicativos nativos não está disponível nesta sessão. Xvfb, AT-SPI privado e CLI não certificam esses aceites.

M07 (busy/idle host refinado), M08 (integração adicional de shells), M13 (extrações incrementais) e expansões de agentes/perfis/gráficos inline continuam como melhorias posteriores, sem transformar o Marco 5 inteiro em bloqueador da versão.
