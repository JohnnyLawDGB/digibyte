load("secp256k1_params.sage")

<<<<<<< HEAD
orders_done = set()
results = {}
first = True
for b in range(1, P):
    # There are only 6 curves (up to isomorphism) of the form y^2=x^3+B. Stop once we have tried all.
=======
MAX_ORDER = 1000

# Set of (curve) orders we have encountered so far.
orders_done = set()

# Map from (subgroup) orders to [b, int(gen.x), int(gen.y), gen, lambda] for those subgroups.
solutions = {}

# Iterate over curves of the form y^2 = x^3 + B.
for b in range(1, P):
    # There are only 6 curves (up to isomorphism) of the form y^2 = x^3 + B. Stop once we have tried all.
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    if len(orders_done) == 6:
        break

    E = EllipticCurve(F, [0, b])
    print("Analyzing curve y^2 = x^3 + %i" % b)
    n = E.order()
<<<<<<< HEAD
    # Skip curves with an order we've already tried
    if n in orders_done:
        print("- Isomorphic to earlier curve")
        continue
    orders_done.add(n)
    # Skip curves isomorphic to the real secp256k1
    if n.is_pseudoprime():
        print(" - Isomorphic to secp256k1")
        continue

    print("- Finding subgroups")

    # Find what prime subgroups exist
    for f, _ in n.factor():
        print("- Analyzing subgroup of order %i" % f)
        # Skip subgroups of order >1000
        if f < 4 or f > 1000:
            print("  - Bad size")
            continue

        # Iterate over X coordinates until we find one that is on the curve, has order f,
        # and for which curve isomorphism exists that maps it to X coordinate 1.
        for x in range(1, P):
            # Skip X coordinates not on the curve, and construct the full point otherwise.
            if not E.is_x_coord(x):
                continue
            G = E.lift_x(F(x))

            print("  - Analyzing (multiples of) point with X=%i" % x)

            # Skip points whose order is not a multiple of f. Project the point to have
            # order f otherwise.
            if (G.order() % f):
                print("    - Bad order")
                continue
            G = G * (G.order() // f)

            # Find lambda for endomorphism. Skip if none can be found.
            lam = None
            for l in Integers(f)(1).nth_root(3, all=True):
                if int(l)*G == E(BETA*G[0], G[1]):
                    lam = int(l)
                    break
            if lam is None:
                print("    - No endomorphism for this subgroup")
                break

            # Now look for an isomorphism of the curve that gives this point an X
            # coordinate equal to 1.
            # If (x,y) is on y^2 = x^3 + b, then (a^2*x, a^3*y) is on y^2 = x^3 + a^6*b.
            # So look for m=a^2=1/x.
            m = F(1)/G[0]
            if not m.is_square():
                print("    - No curve isomorphism maps it to a point with X=1")
                continue
            a = m.sqrt()
            rb = a^6*b
            RE = EllipticCurve(F, [0, rb])

            # Use as generator twice the image of G under the above isormorphism.
            # This means that generator*(1/2 mod f) will have X coordinate 1.
            RG = RE(1, a^3*G[1]) * 2
            # And even Y coordinate.
            if int(RG[1]) % 2:
                RG = -RG
            assert(RG.order() == f)
            assert(lam*RG == RE(BETA*RG[0], RG[1]))

            # We have found curve RE:y^2=x^3+rb with generator RG of order f. Remember it
            results[f] = {"b": rb, "G": RG, "lambda": lam}
            print("    - Found solution")
            break

    print("")

print("")
print("")
print("/* To be put in src/group_impl.h: */")
first = True
for f in sorted(results.keys()):
    b = results[f]["b"]
    G = results[f]["G"]
    print("#  %s EXHAUSTIVE_TEST_ORDER == %i" % ("if" if first else "elif", f))
    first = False
    print("static const secp256k1_ge secp256k1_ge_const_g = SECP256K1_GE_CONST(")
    print("    0x%08x, 0x%08x, 0x%08x, 0x%08x," % tuple((int(G[0]) >> (32 * (7 - i))) & 0xffffffff for i in range(4)))
    print("    0x%08x, 0x%08x, 0x%08x, 0x%08x," % tuple((int(G[0]) >> (32 * (7 - i))) & 0xffffffff for i in range(4, 8)))
    print("    0x%08x, 0x%08x, 0x%08x, 0x%08x," % tuple((int(G[1]) >> (32 * (7 - i))) & 0xffffffff for i in range(4)))
    print("    0x%08x, 0x%08x, 0x%08x, 0x%08x" % tuple((int(G[1]) >> (32 * (7 - i))) & 0xffffffff for i in range(4, 8)))
    print(");")
    print("static const secp256k1_fe secp256k1_fe_const_b = SECP256K1_FE_CONST(")
    print("    0x%08x, 0x%08x, 0x%08x, 0x%08x," % tuple((int(b) >> (32 * (7 - i))) & 0xffffffff for i in range(4)))
    print("    0x%08x, 0x%08x, 0x%08x, 0x%08x" % tuple((int(b) >> (32 * (7 - i))) & 0xffffffff for i in range(4, 8)))
    print(");")
print("#  else")
print("#    error No known generator for the specified exhaustive test group order.")
print("#  endif")

print("")
print("")
print("/* To be put in src/scalar_impl.h: */")
first = True
for f in sorted(results.keys()):
    lam = results[f]["lambda"]
=======

    # Skip curves with an order we've already tried
    if n in orders_done:
        print("- Isomorphic to earlier curve")
        print()
        continue
    orders_done.add(n)

    # Skip curves isomorphic to the real secp256k1
    if n.is_pseudoprime():
        assert E.is_isomorphic(C)
        print("- Isomorphic to secp256k1")
        print()
        continue

    print("- Finding prime subgroups")

    # Map from group_order to a set of independent generators for that order.
    curve_gens = {}

    for g in E.gens():
        # Find what prime subgroups of group generated by g exist.
        g_order = g.order()
        for f, _ in g.order().factor():
            # Skip subgroups that have bad size.
            if f < 4:
                print(f"  - Subgroup of size {f}: too small")
                continue
            if f > MAX_ORDER:
                print(f"  - Subgroup of size {f}: too large")
                continue

            # Construct a generator for that subgroup.
            gen = g * (g_order // f)
            assert(gen.order() == f)

            # Add to set the minimal multiple of gen.
            curve_gens.setdefault(f, set()).add(min([j*gen for j in range(1, f)]))
            print(f"  - Subgroup of size {f}: ok")

    for f in sorted(curve_gens.keys()):
        print(f"- Constructing group of order {f}")
        cbrts = sorted([int(c) for c in Integers(f)(1).nth_root(3, all=true) if c != 1])
        gens = list(curve_gens[f])
        sol_count = 0
        no_endo_count = 0

        # Consider all non-zero linear combinations of the independent generators.
        for j in range(1, f**len(gens)):
            gen = sum(gens[k] * ((j // f**k) % f) for k in range(len(gens)))
            assert not gen.is_zero()
            assert (f*gen).is_zero()

            # Find lambda for endomorphism. Skip if none can be found.
            lam = None
            for l in cbrts:
                if l*gen == E(BETA*gen[0], gen[1]):
                    lam = l
                    break

            if lam is None:
                no_endo_count += 1
            else:
                sol_count += 1
                solutions.setdefault(f, []).append((b, int(gen[0]), int(gen[1]), gen, lam))

        print(f"  - Found {sol_count} generators (plus {no_endo_count} without endomorphism)")

    print()

def output_generator(g, name):
    print(f"#define {name} SECP256K1_GE_CONST(\\")
    print("    0x%08x, 0x%08x, 0x%08x, 0x%08x,\\" % tuple((int(g[0]) >> (32 * (7 - i))) & 0xffffffff for i in range(4)))
    print("    0x%08x, 0x%08x, 0x%08x, 0x%08x,\\" % tuple((int(g[0]) >> (32 * (7 - i))) & 0xffffffff for i in range(4, 8)))
    print("    0x%08x, 0x%08x, 0x%08x, 0x%08x,\\" % tuple((int(g[1]) >> (32 * (7 - i))) & 0xffffffff for i in range(4)))
    print("    0x%08x, 0x%08x, 0x%08x, 0x%08x\\" % tuple((int(g[1]) >> (32 * (7 - i))) & 0xffffffff for i in range(4, 8)))
    print(")")

def output_b(b):
    print(f"#define SECP256K1_B {int(b)}")

print()
print("To be put in src/group_impl.h:")
print()
print("/* Begin of section generated by sage/gen_exhaustive_groups.sage. */")
for f in sorted(solutions.keys()):
    # Use as generator/2 the one with lowest b, and lowest (x, y) generator (interpreted as non-negative integers).
    b, _, _, HALF_G, lam = min(solutions[f])
    output_generator(2 * HALF_G, f"SECP256K1_G_ORDER_{f}")
print("/** Generator for secp256k1, value 'g' defined in")
print(" *  \"Standards for Efficient Cryptography\" (SEC2) 2.7.1.")
print(" */")
output_generator(G, "SECP256K1_G")
print("/* These exhaustive group test orders and generators are chosen such that:")
print(" * - The field size is equal to that of secp256k1, so field code is the same.")
print(" * - The curve equation is of the form y^2=x^3+B for some small constant B.")
print(" * - The subgroup has a generator 2*P, where P.x is as small as possible.")
print(f" * - The subgroup has size less than {MAX_ORDER} to permit exhaustive testing.")
print(" * - The subgroup admits an endomorphism of the form lambda*(x,y) == (beta*x,y).")
print(" */")
print("#if defined(EXHAUSTIVE_TEST_ORDER)")
first = True
for f in sorted(solutions.keys()):
    b, _, _, _, lam = min(solutions[f])
    print(f"#  {'if' if first else 'elif'} EXHAUSTIVE_TEST_ORDER == {f}")
    first = False
    print()
    print(f"static const secp256k1_ge secp256k1_ge_const_g = SECP256K1_G_ORDER_{f};")
    output_b(b)
    print()
print("#  else")
print("#    error No known generator for the specified exhaustive test group order.")
print("#  endif")
print("#else")
print()
print("static const secp256k1_ge secp256k1_ge_const_g = SECP256K1_G;")
output_b(7)
print()
print("#endif")
print("/* End of section generated by sage/gen_exhaustive_groups.sage. */")


print()
print()
print("To be put in src/scalar_impl.h:")
print()
print("/* Begin of section generated by sage/gen_exhaustive_groups.sage. */")
first = True
for f in sorted(solutions.keys()):
    _, _, _, _, lam = min(solutions[f])
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    print("#  %s EXHAUSTIVE_TEST_ORDER == %i" % ("if" if first else "elif", f))
    first = False
    print("#    define EXHAUSTIVE_TEST_LAMBDA %i" % lam)
print("#  else")
print("#    error No known lambda for the specified exhaustive test group order.")
print("#  endif")
<<<<<<< HEAD
print("")
=======
print("/* End of section generated by sage/gen_exhaustive_groups.sage. */")
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
