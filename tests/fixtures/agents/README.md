# Imagens sintéticas de validação

Criadas para testar recebimento multimodal sem dados pessoais. O código visível
não deve aparecer no prompt enviado ao modelo; a resposta precisa vir dos pixels.

| Arquivo | Conteúdo esperado |
| --- | --- |
| ação PNG's image.png | quadrado vermelho, círculo azul, K7R4 |
| JPEG image.jpg | triângulo amarelo, retângulo roxo, M2X9 |
| clipboard.png | triângulo verde, retângulo laranja, V8C6 |

Todos têm fundo branco e dimensões 520×280. Acentos, espaço e apóstrofo no nome do
PNG exercitam a importação; o caminho armazenado no cache recebe um UUID próprio.
