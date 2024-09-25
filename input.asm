beq $0, $0, skip
changeMe: .word 0
skip: lis $3
.word 241
lis $4
.word changeMe
sw $3, 0($4)
jr $31