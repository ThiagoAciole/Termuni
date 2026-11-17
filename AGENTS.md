# Orientações para agentes

O Termuni é um terminal Linux em C, GTK 4, libadwaita, libghostty-vt e Meson. Preserve o fluxo do GNOME Builder e a execução em Wayland. Trabalhe na raiz que contém este arquivo: `/home/thiago/Projects/Termuni`.

## Comece aqui

1. Leia o quadro **Estado atual** em [PLANO_IMPLEMENTACAO.md](docs/PLANO_IMPLEMENTACAO.md) para identificar o marco em andamento. As melhorias priorizadas e seus critérios ficam em [docs/ROADMAP.md](docs/ROADMAP.md); o índice de evidências, checklists e histórico fica em [docs/README.md](docs/README.md). Registros antigos não substituem o quadro atual.
2. Para produto, componentes, build e riscos técnicos, consulte [PDR.Arquiteture.md](docs/architecture/PDR.Arquiteture.md). Para aparência, interações e referências, consulte [DESIGN.md](docs/design/DESIGN.md).
3. Confira o estado atual dos arquivos e do Git antes de editar. Em 09/11/2026, a árvore estava sem commit inicial e os arquivos apareciam como não rastreados; preserve todo o trabalho existente.
4. Ao concluir um incremento, atualize o quadro atual do plano com o que mudou, como foi verificado e o que permanece pendente. Marque como concluído somente o que tiver evidência compatível com o critério do marco.

## Limites do projeto

- Uma aba representa uma sessão própria. A faixa de abas fica oculta com apenas uma aba e aparece com mais de uma.
- Use a revisão e o Zig fixados no Meson/wrap. Confirme o build no ambiente disponível; não trate compilação nativa como prova de execução no GNOME Builder ou de interação visual.
- Preserve os perfis `development` e `default`, seus IDs, schemas e ícones. A instalação antiga foi removida a pedido do usuário; desenvolvimento deve usar o build do projeto.
- O widget de terminal reúne PTY, motor, renderização e entrada no arquivo atual. Confira sua semântica antes de separá-lo em módulos ou alterar entrada, redimensionamento e encerramento.
- A sessão de teste pode conter trabalho do usuário. Antes de reiniciá-la, informe que o shell e suas abas serão encerrados. Para manter o app aberto após o build, inicie `build/development/src/termuni` sem `G_DEBUG=fatal-warnings`: um aviso GDK de apresentação já encerrou o processo com essa variável.
- Relate o resultado observado, não somente o código escrito. Se a inspeção visual ou o GNOME Builder estiverem indisponíveis, registre essa limitação.
