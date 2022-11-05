
.text
.arch   armv8-a+crypto

// AES-128-ECB Assembly language
// x0: key address, the keys are already expanded
// x1: address of the page
// x2: size assume it has been 128-bit padded
// x3: enc or dec, 0 is enc and 1 is dec
// each time we process 128bits and replace it.
// void aes128_block(uint32_t *expandedkeys, uint64_t *data, size_t size, uint32_t isdec);

.global	aes128_block
.type	aes128_block,%function

.align  2
aes128_block:

.Laes128prolog:
    stp x29, x30, [sp,#-64]!
    mov x29, sp
    str q8, [sp, #16]
    str q9, [sp, #32]
    str q10, [sp, #48]

// load keys
    ld1 {v1.16b-v4.16b}, [x0], #64
    ld1 {v5.16b-v8.16b}, [x0], #64
    ld1 {v9.16b-v11.16b}, [x0], #48

// branch?
    cmp x3,xzr
    b.eq .Lenc
    b.ne .Ldec

.Lenc:
    //v1-v11:keys
    add x2, x1, x2
.Lloop1:
    ld1 {v0.16b},[x1]
    aese v0.16b,v1.16b
    aesmc v0.16b,v0.16b
    aese v0.16b,v2.16b
    aesmc v0.16b,v0.16b
    aese v0.16b,v3.16b
    aesmc v0.16b,v0.16b
    aese v0.16b,v4.16b
    aesmc v0.16b,v0.16b    
    aese v0.16b,v5.16b
    aesmc v0.16b,v0.16b
    aese v0.16b,v6.16b
    aesmc v0.16b,v0.16b
    aese v0.16b,v7.16b
    aesmc v0.16b,v0.16b
    aese v0.16b,v8.16b
    aesmc v0.16b,v0.16b
    aese v0.16b,v9.16b
    aesmc v0.16b,v0.16b
    aese v0.16b,v10.16b
    eor v0.16b,v0.16b,v11.16b
    //store the results in v0 to x1 and x1+8
    st1 {v0.16b}, [x1]
    add x1, x1, #16
    cmp x1, x2
    b.lt .Lloop1
    b .Laes128epilog //finished, jump to the end

.Ldec:
    //v1-v11:keys
    aesimc v2.16b,v2.16b
    aesimc v3.16b,v3.16b
    aesimc v4.16b,v4.16b
    aesimc v5.16b,v5.16b
    aesimc v6.16b,v6.16b
    aesimc v7.16b,v7.16b
    aesimc v8.16b,v8.16b
    aesimc v9.16b,v9.16b
    aesimc v10.16b,v10.16b
    //v1-v11:keys
    add x2, x1, x2
.Lloop2:
    ld1 {v0.16b},[x1]
    aesd v0.16b,v11.16b
    aesimc v0.16b,v0.16b
    aesd v0.16b,v10.16b
    aesimc v0.16b,v0.16b
    aesd v0.16b,v9.16b
    aesimc v0.16b,v0.16b
    aesd v0.16b,v8.16b
    aesimc v0.16b,v0.16b
    aesd v0.16b,v7.16b
    aesimc v0.16b,v0.16b
    aesd v0.16b,v6.16b
    aesimc v0.16b,v0.16b
    aesd v0.16b,v5.16b
    aesimc v0.16b,v0.16b
    aesd v0.16b,v4.16b
    aesimc v0.16b,v0.16b
    aesd v0.16b,v3.16b
    aesimc v0.16b,v0.16b
    aesd v0.16b,v2.16b
    eor v0.16b,v0.16b,v1.16b
    //store the results in v0 to x1 and x1+8
    st1 {v0.16b}, [x1]
    add x1, x1, #16
    cmp x1, x2
    b.lt .Lloop2
    b .Laes128epilog //finished, jump to the end   


.Laes128epilog:
    ldr q10, [sp, #48]
    ldr q9, [sp, #32]
    ldr q8, [sp, #16]
    ldr x29, [sp], #64
    ret