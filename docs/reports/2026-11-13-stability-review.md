# Revisão arquitetural e estabilidade — 13/11/2026

Base `51fa0b4`, branch `chore/stability-criteria`. A solicitação foi confrontar recomendações com o código real, atualizar os Marcos 4/5 e separar o aceite estável de melhorias opcionais, incluindo TUI moderna, Unicode/emoji e True Color.

## Resultado da auditoria

- **Atalhos já implementados:** GAction/GSimpleAction, GSettings, padrões editáveis, restauração e conflitos. Não foram reimplementados. A lista de nomes reservados na janela ainda é um ponto de manutenção; o encoder continua responsável pelas teclas do shell.
- **Sessões já versionadas:** v3 com conversão v2→v3 quando não existe v3. A origem nunca é apagada. O teste foi ampliado para comparar os bytes do v2 e provar que um v2 posterior inválido não sobrescreve o v3 numa segunda ativação.
- **Falha de gravação exercitada:** diretório no caminho do v3 força erro real. A migração continua recuperando os dados válidos em memória e mantém os bytes originais do v2.
- **Agentes já separados:** os adaptadores existentes e os testes sem executáveis mantêm o terminal utilizável sem Claude/Codex. Nenhum modelo foi chamado nesta rodada.
- **Vazamentos:** o diagnóstico histórico de ~40 KB não é tratado como crescimento progressivo. P0 comparou churn 3/15, corrigiu referência de ícone e separou residual constante de bibliotecas. Valgrind não foi repetido nesta rodada; a versão candidata ainda exige comparação atual.

## Correção funcional encontrada

`write_session_file` alocava zero bytes ao salvar uma lista sem janelas. `g_malloc(0)` retorna NULL, rejeitado por `g_variant_store`. `/ui/save-empty-session` reproduziu o erro crítico e passou após garantir um destino não nulo, mantendo uma serialização válida de zero bytes. O mesmo erro apareceu ao ampliar o cenário de migração.

A gravação usa o wrapper GLib existente. A confirmação da falha e a preservação da origem não equivalem a durabilidade contra queda de energia na primeira criação. M15 registra esse trabalho; referência e limites estão nos [critérios de estabilidade](../checklists/stability.md).

## Capacidades verificadas

| Cenário | Observação |
| --- | --- |
| `/vt/truecolor` | SGR 24-bit preservou foreground 17/83/149 e background 203/41/7, sem conversão para a paleta. |
| `/ui/truecolor-unicode-render` | CJK, acento combinado e emojis preservados na linha VT; pixel opaco do fundo GTK exatamente RGB 203/41/7. Captura aberta e inspecionada, com emojis coloridos. |
| `/ui/modern-tui` | btop 1.4.7 instalado; CPU/memória em configuração isolada, tela alternativa, resize, saída com q e retorno ao shell. Painel de comandos dos processos excluído da captura. |
| Migração / sessão vazia | Idempotência, origem byte a byte, falha de escrita e salvar zero janelas passaram em execução direcionada. |

Capturas temporárias: `/tmp/termac-stability-shots/truecolor-unicode.png` e `modern-tui-btop.png`. Configuração btop de teste ativa True Color; anunciar COLORTERM para autodetecção de outros programas continua M14. Não se declara compatibilidade irrestrita com toda TUI, fonte ou compositor.

## Verificação e limites

- `meson test -C build`: **11/11**; UI normal 173,88 s e HiDPI 188,51 s, 49 casos UI registrados. Os quatro auxiliares opt-in anteriores permanecem pulados na execução comum; btop estava instalado e o novo cenário executou nos dois fatores de escala.
- Cinco cenários UI direcionados passaram, incluindo a regressão RED→GREEN da sessão vazia. `/vt/truecolor` passou junto da suíte VT.
- Perfil default: util/vt/settings **3/3** e compilação completa passaram. As traduções continuam 157, sem fuzzy.
- Os limites de tempo UI foram ampliados para 240/300 s devido aos novos cenários; isso não é medição de desempenho do emulador.

Builder, entrega/clique de notificação GNOME, imagem real pelo Flatpak, IME/escala fracionária/Orca e reinício entre processos permanecem pendentes. A validação GUI usa Xvfb e XDG isolados, sem reiniciar janelas pessoais.

Contrato de lançamento: [checklist de estabilidade](../checklists/stability.md). Recursos opcionais do Marco 5 não bloqueiam integralmente a versão; crashes, corrupção ou falhas graves de entrada bloqueiam independentemente do componente.
