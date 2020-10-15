/*
 * (C) 2007-2017 Alibaba Group Holding Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * See the AUTHORS file for names of contributors.
 *
 */

#ifndef TAIR_RWLOCK_HPP
#define TAIR_RWLOCK_HPP
#ifdef CONFIG_ARM64_LSE_ATOMICS
#define ARM64_LSE_ATOMIC_INSN(llsc, lse)				\
	ALTERNATIVE(llsc, __LSE_PREAMBLE lse, ARM64_HAS_LSE_ATOMICS)
#else	/* CONFIG_ARM64_LSE_ATOMICS */
#define ARM64_LSE_ATOMIC_INSN(llsc, lse)	llsc
#endif
#if defined(__i386__) || defined(__x86_64__) || defined(__aarch64__)
namespace tair {

class rwlock_t {
public:
#ifdef __aarch64__
    rwlock_t() {
        lock = 0;
    }
    void read_lock(){
	unsigned int tmp,tmp2;
	asm volatile(
 	"	sevl\n"
	ARM64_LSE_ATOMIC_INSN(
	/* LL/SC */
 	"1:	wfe\n"
 	"2:	ldaxr	%w0, %2\n"
 	"	add	%w0, %w0, #1\n"
 	"	tbnz	%w0, #31, 1b\n"
 	"	stxr	%w1, %w0, %2\n"
	"	nop\n"
	"	cbnz	%w1, 2b",
	/* LSE atomics */
	"1:	wfe\n"
	"2:	ldr	%w0, %2\n"
	"	adds	%w1, %w0, #1\n"
	"	tbnz	%w1, #31, 1b\n"
	"	casa	%w0, %w1, %2\n"
	"	sbc	%w0, %w1, %w0\n"
	"	cbnz	%w0, 2b")
 	: "=&r" (tmp), "=&r" (tmp2), "+Q" (this->lock)
 	:
	: "cc", "memory");
   }

   bool read_trylock(){
 	unsigned int tmp, tmp2;
 
	asm volatile(ARM64_LSE_ATOMIC_INSN(
	/* LL/SC */
	"	mov	%w1, #1\n"
 	"	ldaxr	%w0, %2\n"
 	"	add	%w0, %w0, #1\n"
 	"	tbnz	%w0, #31, 1f\n"
 	"	stxr	%w1, %w0, %2\n"
	"1:",
	/* LSE atomics */
	"	ldr	%w0, %2\n"
	"	adds	%w1, %w0, #1\n"
	"	tbnz	%w1, #31, 1f\n"
	"	casa	%w0, %w1, %2\n"
	"	sbc	%w1, %w1, %w0\n"
	"1:")
	: "=&r" (tmp), "=&r" (tmp2), "+Q" (this->lock)
 	:
	: "cc", "memory");
 
	return !tmp2;
   }
   void read_unlock()
   {
 	unsigned int tmp, tmp2;
 
	asm volatile(ARM64_LSE_ATOMIC_INSN(
	/* LL/SC */
 	"1:	ldxr	%w0, %2\n"
 	"	sub	%w0, %w0, #1\n"
 	"	stlxr	%w1, %w0, %2\n"
	"	cbnz	%w1, 1b",
	/* LSE atomics */
	"	movn	%w0, #0\n"
	"	nop\n"
	"	nop\n"
	"	staddl	%w0, %2")
 	: "=&r" (tmp), "=&r" (tmp2), "+Q" (this->lock)
 	:
 	: "memory");
   }

   void write_lock(){
 	unsigned int tmp;

	asm volatile(ARM64_LSE_ATOMIC_INSN(
	/* LL/SC */
	"	sevl\n"
	"1:	wfe\n"
	"2:	ldaxr	%w0, %1\n"
	"	cbnz	%w0, 1b\n"
	"	stxr	%w0, %w2, %1\n"
	"	cbnz	%w0, 2b\n"
	"       nop",
	/* LSE atomics */
	"1:	mov	%w0, wzr\n"
	"2:	casa	%w0, %w2, %1\n"
	"	cbz	%w0, 3f\n"
	"	ldxr	%w0, %1\n"
	"	cbz	%w0, 2b\n"
	"	wfe\n"
	"	b	1b\n"
	"3:")
	: "=&r" (tmp), "+Q" (this->lock)
	: "r" (0x80000000)
	: "memory");
   }
   bool write_trylock(){
	unsigned int tmp;
  
	asm volatile(ARM64_LSE_ATOMIC_INSN(
	/* LL/SC */
 	"	ldaxr	%w0, %1\n"
 	"	cbnz	%w0, 1f\n"
 	"	stxr	%w0, %w2, %1\n"
	"1:",
	/* LSE atomics */
	"	mov	%w0, wzr\n"
	"	casa	%w0, %w2, %1\n"
	"	nop")
 	: "=&r" (tmp), "+Q" (this->lock)
 	: "r" (0x80000000)
 	: "memory");
	return !tmp;
  }

   void write_unlock(){
	asm volatile(ARM64_LSE_ATOMIC_INSN(
	"	stlr	wzr, %0",
	"	swpl	wzr, wzr, %0")
	: "=Q" (this->lock) :: "memory");
   }
#else
    rwlock_t() {
        lock = RW_LOCK_BIAS;
    }

    void read_lock() {
        asm volatile(
        "1:"
                " lock; decl (%0)\n\t"
                " jns 3f\n\t"
                " lock; incl (%0)\n\t"
                "2:\n\t"
                " pause\n\t"
                " cmpl $1, (%0)\n\t"
                " js 2b\n\t"
                " jmp 1b\n\t"
                "3:\n\t"
        :
        :"r"(&this->lock)
        :"memory"
        );
    }

    bool read_trylock() {
        bool ret = false;
        asm volatile (
        " lock; decl (%1)\n\t"
                " setns %0\n\t"
                " jns 1f\n\t"
                " lock; incl (%1)\n\t"
                "1:\n\t"
        : "=r"(ret)
        : "r"(&this->lock)
        : "memory"
        );
        return ret;
    }

    void read_unlock() {
        asm volatile ( "lock; incl (%0)\n\t" : : "r"(&this->lock) : "memory");
    }

    void write_lock() {
        asm volatile (
        "1:\n\t"
                " lock; subl %1, (%0)\n\t"
                " jns 3f\n\t"
                " lock; addl %1, (%0)\n\t"
                "2:\n\t"
                " pause\n\t"
                " cmpl %1, (%0)\n\t"
                " js 2b\n\t"
                " jmp 1b\n\t"
                "3:\n\t"
        :
        : "r"(&this->lock), "i"(RW_LOCK_BIAS)
        : "memory"
        );
    }

    bool write_trylock() {
        bool ret = false;
        asm volatile (
        " lock; subl %2, (%1)\n\t"
                " setns %0\n\t"
                " jns 1f\n\t"
                " lock; addl %2, (%1)\n\t"
                "1:\n\t"
        : "=r"(ret)
        : "r"(&this->lock), "i"(RW_LOCK_BIAS)
        : "memory"
        );
        return ret;
    }

    void write_unlock() {
        asm volatile (
        "lock; addl %1, %0\n\t"
        : "+m"(this->lock)
        : "i"(RW_LOCK_BIAS)
        : "memory"
        );
    }
#endif
private:
    static const int RW_LOCK_BIAS = 0x100000;
    volatile int lock;
};

class read_guard_t {
public:
    read_guard_t(rwlock_t *lock) : lock_(lock) {
        lock_->read_lock();
    }

    ~read_guard_t() {
        lock_->read_unlock();
    }

private:
    rwlock_t *lock_;
};

class write_guard_t {
public:
    write_guard_t(rwlock_t *lock) : lock_(lock) {
        lock_->write_lock();
    }

    ~write_guard_t() {
        lock_->write_unlock();
    }

private:
    rwlock_t *lock_;
};

}
#else // defined(__i386__) || defined(__x86_64__)
#error "tair::rwlock_t cannot compile in this architecture!"
#endif // defined(__i386__) || defined(__x86_64__)

#endif
