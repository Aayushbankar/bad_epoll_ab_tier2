import subprocess

html_content = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<style>
@page {
    size: 1080px 1350px;
    margin: 0;
}
* {
    box-sizing: border-box;
    margin: 0;
    padding: 0;
}
body {
    font-family: -apple-system, BlinkMacSystemFont, "SF Pro Display", "Inter", "Helvetica Neue", Arial, sans-serif;
    background-color: #0E1015;
    color: #EDEDED;
    -webkit-font-smoothing: antialiased;
}
.slide {
    width: 1080px;
    height: 1350px;
    page-break-after: always;
    position: relative;
    padding: 90px 85px;
    background-color: #0E1015;
    overflow: hidden;
    display: flex;
    flex-direction: column;
    justify-content: space-between;
}
/* Top Meta Header */
.top-meta {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    border-bottom: 1px solid #222630;
    padding-bottom: 24px;
    margin-bottom: 48px;
}
.meta-tag {
    font-family: "SF Mono", "Source Code Pro", "JetBrains Mono", Menlo, monospace;
    font-size: 17px;
    letter-spacing: 2.5px;
    color: #D97706; /* warm amber accent */
    text-transform: uppercase;
    font-weight: 600;
}
.page-index {
    font-family: "SF Mono", monospace;
    font-size: 17px;
    color: #525866;
}

/* Typography */
h1 {
    font-size: 66px;
    font-weight: 800;
    line-height: 1.1;
    letter-spacing: -1.5px;
    color: #FFFFFF;
    margin-bottom: 24px;
}
h2 {
    font-size: 48px;
    font-weight: 800;
    line-height: 1.15;
    letter-spacing: -1px;
    color: #FFFFFF;
    margin-bottom: 20px;
}
p.lead {
    font-size: 25px;
    line-height: 1.45;
    color: #949BA8;
    margin-bottom: 36px;
    font-weight: 400;
}

/* Editorial Cards */
.box {
    background: #141720;
    border: 1px solid #222630;
    border-radius: 12px;
    padding: 32px 36px;
    margin-bottom: 24px;
}
.box-accent {
    border-left: 4px solid #D97706;
}
.box-red {
    border-left: 4px solid #EF4444;
}
.box-green {
    border-left: 4px solid #10B981;
}

/* Metrics Split */
.metrics-row {
    display: flex;
    gap: 20px;
    margin-bottom: 28px;
}
.metric-card {
    flex: 1;
    background: #141720;
    border: 1px solid #222630;
    border-radius: 12px;
    padding: 28px 30px;
}
.metric-val {
    font-family: "SF Mono", "Source Code Pro", monospace;
    font-size: 46px;
    font-weight: 700;
    line-height: 1;
    margin-bottom: 10px;
    letter-spacing: -1px;
}
.metric-lbl {
    font-size: 17px;
    color: #717786;
    text-transform: uppercase;
    letter-spacing: 1px;
    font-weight: 600;
}

/* Lists */
.entry-list {
    display: flex;
    flex-direction: column;
    gap: 20px;
}
.entry {
    display: flex;
    gap: 20px;
    align-items: flex-start;
}
.entry-num {
    font-family: "SF Mono", monospace;
    font-size: 19px;
    font-weight: 700;
    color: #D97706;
    background: rgba(217, 119, 6, 0.1);
    padding: 4px 10px;
    border-radius: 6px;
    flex-shrink: 0;
}
.entry-body {
    font-size: 22px;
    line-height: 1.45;
    color: #D1D5DB;
}
.entry-body strong {
    color: #FFFFFF;
}

/* Monospace Snippets */
.mono-snip {
    font-family: "SF Mono", "Source Code Pro", monospace;
    font-size: 19px;
    background: #080A0E;
    border: 1px solid #1E232E;
    padding: 16px 20px;
    border-radius: 8px;
    color: #38BDF8;
    line-height: 1.4;
    margin: 14px 0;
}

/* Table Style */
.data-table {
    width: 100%;
    border-collapse: collapse;
    margin-top: 10px;
}
.data-table th {
    text-align: left;
    font-family: "SF Mono", monospace;
    font-size: 16px;
    color: #6B7280;
    text-transform: uppercase;
    letter-spacing: 1px;
    padding: 12px 14px;
    border-bottom: 1px solid #222630;
}
.data-table td {
    font-size: 20px;
    padding: 18px 14px;
    border-bottom: 1px solid #1A1E27;
    color: #D1D5DB;
    line-height: 1.35;
}
.data-table td.code {
    font-family: "SF Mono", monospace;
    font-size: 18px;
    color: #F59E0B;
}

/* Footer */
.bottom-meta {
    display: flex;
    justify-content: space-between;
    align-items: center;
    border-top: 1px solid #222630;
    padding-top: 24px;
    font-size: 17px;
    color: #525866;
}
.author-sig {
    font-weight: 600;
    color: #8890A0;
}
.swipe-action {
    color: #D97706;
    font-weight: 600;
    font-family: "SF Mono", monospace;
    font-size: 15px;
    letter-spacing: 0.5px;
}
</style>
</head>
<body>

<!-- SLIDE 1: COVER -->
<div class="slide">
    <div class="top-meta">
        <div class="meta-tag">Research Case Study</div>
        <div class="page-index">01 / 08</div>
    </div>

    <div>
        <div style="font-family: 'SF Mono', monospace; font-size: 20px; color: #D97706; font-weight: 600; margin-bottom: 16px; letter-spacing: 1px;">
            CVE-2026-46242 · BAD EPOLL
        </div>
        <h1>
            Root on Linux.<br>
            <span style="color: #64748B;">DoS on Android.</span>
        </h1>
        <p class="lead">
            What happens when a 99% reliable kernelCTF root exploit meets real-world ARM64 mobile defenses?
        </p>

        <div class="box box-accent" style="margin-top: 36px;">
            <div style="font-size: 16px; font-family: 'SF Mono', monospace; color: #9CA3AF; text-transform: uppercase; letter-spacing: 1.5px; margin-bottom: 8px;">
                10-Week Empirical Audit
            </div>
            <div style="font-size: 28px; font-weight: 700; color: #FFFFFF; line-height: 1.35;">
                102,740 automated race runs.<br>
                21 dead ends documented.<br>
                <span style="color: #D97706;">Scientifically verified DoS-only verdict.</span>
            </div>
        </div>
    </div>

    <div class="bottom-meta">
        <div class="author-sig">Aayush Bankar · Independent Security Research</div>
        <div class="swipe-action">SWIPE &rarr;</div>
    </div>
</div>

<!-- SLIDE 2: THE PARADOX -->
<div class="slide">
    <div class="top-meta">
        <div class="meta-tag">The Architecture Gap</div>
        <div class="page-index">02 / 08</div>
    </div>

    <div>
        <h2>Two Environments.<br>One Vulnerability.</h2>
        <p class="lead">
            The target is a Use-After-Free in <code style="color: #F59E0B; font-family: 'SF Mono', monospace;">fs/eventpoll.c</code> discovered by Jaeyoung Chung.
        </p>

        <div class="metrics-row">
            <div class="metric-card" style="border-left: 3px solid #10B981;">
                <div class="metric-val" style="color: #10B981;">99%</div>
                <div class="metric-lbl">x86_64 VM Root</div>
            </div>
            <div class="metric-card" style="border-left: 3px solid #EF4444;">
                <div class="metric-val" style="color: #EF4444;">0 / 102k</div>
                <div class="metric-lbl">Android GKI Hits</div>
            </div>
        </div>

        <div class="box">
            <div style="font-size: 20px; font-weight: 700; color: #FFFFFF; margin-bottom: 8px;">
                The Question
            </div>
            <div style="font-size: 21px; line-height: 1.45; color: #9CA3AF;">
                Why does an exploit that easily achieves UID 0 on desktop Linux completely flatline when ported to a hardened mobile testbed?
            </div>
        </div>
    </div>

    <div class="bottom-meta">
        <div class="author-sig">Aayush Bankar</div>
        <div class="swipe-action">SWIPE &rarr;</div>
    </div>
</div>

<!-- SLIDE 3: TIER 1 -->
<div class="slide">
    <div class="top-meta">
        <div class="meta-tag">Tier 1 · x86_64 Linux</div>
        <div class="page-index">03 / 08</div>
    </div>

    <div>
        <h2>Why It Worked on x86_64</h2>
        <p class="lead">
            On standard unhardened Linux VMs, the exploitation chain is textbook precision.
        </p>

        <div class="entry-list">
            <div class="entry">
                <div class="entry-num">1</div>
                <div class="entry-body">
                    <strong>Heap Grooming:</strong> Reclaimed freed <code style="color: #38BDF8; font-family: 'SF Mono'; font-size: 18px;">epitem</code> objects in <code style="color: #38BDF8; font-family: 'SF Mono'; font-size: 18px;">kmalloc-192</code> using <code style="color: #38BDF8; font-family: 'SF Mono'; font-size: 18px;">msg_msg</code> spray.
                </div>
            </div>
            <div class="entry">
                <div class="entry-num">2</div>
                <div class="entry-body">
                    <strong>KASLR Defeat:</strong> Leaked kernel text base through uninitialized heap pointers.
                </div>
            </div>
            <div class="entry">
                <div class="entry-num">3</div>
                <div class="entry-body">
                    <strong>ROP Execution:</strong> Stack pivot into <code style="color: #38BDF8; font-family: 'SF Mono'; font-size: 18px;">commit_creds(&init_cred)</code> to escalate privileges.
                </div>
            </div>
            <div class="entry">
                <div class="entry-num">4</div>
                <div class="entry-body">
                    <strong>Result:</strong> 99% reliable, clean root shell (<code style="color: #10B981; font-family: 'SF Mono'; font-size: 18px;">UID 0</code>).
                </div>
            </div>
        </div>
    </div>

    <div class="bottom-meta">
        <div class="author-sig">Aayush Bankar</div>
        <div class="swipe-action">SWIPE &rarr;</div>
    </div>
</div>

<!-- SLIDE 4: THE TIMING WALL -->
<div class="slide">
    <div class="top-meta">
        <div class="meta-tag">Obstacle 01 · Scheduling</div>
        <div class="page-index">04 / 08</div>
    </div>

    <div>
        <h2>The 125-Nanosecond Trap</h2>
        <p class="lead">
            The first insurmountable barrier on Android GKI was scheduling physics.
        </p>

        <div class="box box-accent">
            <div style="font-size: 17px; font-family: 'SF Mono', monospace; color: #D97706; margin-bottom: 8px;">
                CONFIG_PREEMPT_DYNAMIC = VOLUNTARY
            </div>
            <div style="font-size: 22px; line-height: 1.45; color: #E5E7EB;">
                In <code style="color: #FBBF24; font-family: 'SF Mono';">fs/eventpoll.c</code>, <code style="color: #FBBF24; font-family: 'SF Mono';">cond_resched()</code> was a complete no-op during <code style="color: #FBBF24; font-family: 'SF Mono';">__ep_remove</code>.
            </div>
            <div class="mono-snip">
                Execution Window: 250–550 CPU cycles (~125–275 ns)
            </div>
            <div style="font-size: 20px; line-height: 1.4; color: #9CA3AF;">
                Natural thread scheduling cannot reliably interrupt a 125ns window without artificial breakpoints or debugger traps.
            </div>
        </div>

        <div class="box">
            <div style="font-size: 21px; color: #FFFFFF; font-weight: 600;">
                0 Hits in 102,740 Natural Race Attempts
            </div>
            <div style="font-size: 19px; color: #949BA8; margin-top: 4px;">
                Breakpoints create an illusion of exploitability that vanishes in production.
            </div>
        </div>
    </div>

    <div class="bottom-meta">
        <div class="author-sig">Aayush Bankar</div>
        <div class="swipe-action">SWIPE &rarr;</div>
    </div>
</div>

<!-- SLIDE 5: 4 FAILED CHAINS -->
<div class="slide">
    <div class="top-meta">
        <div class="meta-tag">Obstacle 02 · Primitive Traps</div>
        <div class="page-index">05 / 08</div>
    </div>

    <div>
        <h2>4 Chains. All Dead Ends.</h2>
        <p class="lead">
            Even forcing synthetic race wins, every escalation path hit a mathematical wall.
        </p>

        <div class="entry-list">
            <div class="entry">
                <div class="entry-num" style="color: #EF4444; background: rgba(239, 68, 68, 0.1);">C0</div>
                <div class="entry-body">
                    <strong>Refcount Corruption:</strong> <code style="color: #F87171; font-family: 'SF Mono'; font-size: 18px;">percpu_counter_dec</code> always targeted the valid outer epoll, never the freed inner struct.
                </div>
            </div>
            <div class="entry">
                <div class="entry-num" style="color: #EF4444; background: rgba(239, 68, 68, 0.1);">C1</div>
                <div class="entry-body">
                    <strong>Dual-Watch Leak:</strong> Structurally impossible under the multi-watch constraints in <code style="color: #F87171; font-family: 'SF Mono'; font-size: 18px;">eventpoll.c:826</code>.
                </div>
            </div>
            <div class="entry">
                <div class="entry-num" style="color: #EF4444; background: rgba(239, 68, 68, 0.1);">C2</div>
                <div class="entry-body">
                    <strong>Arbitrary Decrement:</strong> Decrement locked onto <code style="color: #38BDF8; font-family: 'SF Mono'; font-size: 18px;">root_user</code>, leaving only a fixed NULL write at offset 160.
                </div>
            </div>
            <div class="entry">
                <div class="entry-num" style="color: #EF4444; background: rgba(239, 68, 68, 0.1);">C3</div>
                <div class="entry-body">
                    <strong>Structure Target:</strong> Zeroing offset 160 in reachable structs leads directly to a kernel panic.
                </div>
            </div>
        </div>
    </div>

    <div class="bottom-meta">
        <div class="author-sig">Aayush Bankar</div>
        <div class="swipe-action">SWIPE &rarr;</div>
    </div>
</div>

<!-- SLIDE 6: STRUCT AUDIT (EXP-016) -->
<div class="slide">
    <div class="top-meta">
        <div class="meta-tag">Experiment · EXP-016</div>
        <div class="page-index">06 / 08</div>
    </div>

    <div>
        <h2>The Offset 160 Audit</h2>
        <p class="lead">
            Can zeroing 8 bytes at offset 160 in <code style="color: #F59E0B; font-family: 'SF Mono'; font-size: 21px;">kmalloc-192</code> yield anything other than a crash?
        </p>

        <table class="data-table">
            <thead>
                <tr>
                    <th>Reachable Struct</th>
                    <th>Offset 160 Field</th>
                    <th>Observed Behavior</th>
                </tr>
            </thead>
            <tbody>
                <tr>
                    <td class="code">fib6_info</td>
                    <td>fib6_metrics ptr</td>
                    <td><strong style="color: #EF4444;">Panic:</strong> Null deref on route lookup</td>
                </tr>
                <tr>
                    <td class="code">snd_timer_user</td>
                    <td>queue link ptr</td>
                    <td><strong style="color: #EF4444;">Panic:</strong> Corrupted list traversal</td>
                </tr>
                <tr>
                    <td class="code">packet_fanout</td>
                    <td>next member ptr</td>
                    <td><strong style="color: #EF4444;">Panic:</strong> Teardown deref crash</td>
                </tr>
            </tbody>
        </table>

        <div class="box" style="margin-top: 32px; border-left: 3px solid #EF4444;">
            <div style="font-size: 21px; line-height: 1.4; color: #D1D5DB;">
                <strong>Conclusion:</strong> No reachable <code style="color: #F59E0B; font-family: 'SF Mono'; font-size: 18px;">kmalloc-192</code> struct survives NULL pointer corruption at offset 160.
            </div>
        </div>
    </div>

    <div class="bottom-meta">
        <div class="author-sig">Aayush Bankar</div>
        <div class="swipe-action">SWIPE &rarr;</div>
    </div>
</div>

<!-- SLIDE 7: MITIGATIONS -->
<div class="slide">
    <div class="top-meta">
        <div class="meta-tag">Mitigation Verification</div>
        <div class="page-index">07 / 08</div>
    </div>

    <div>
        <h2>Where Defenses Succeeded</h2>
        <p class="lead">
            Android's defense-in-depth stack performed exactly as designed.
        </p>

        <div class="entry-list">
            <div class="entry">
                <div class="entry-num" style="color: #38BDF8; background: rgba(56, 189, 248, 0.1);">PAC</div>
                <div class="entry-body">
                    <strong>Pointer Authentication:</strong> Neutralized all function pointer overrides and forged return addresses.
                </div>
            </div>
            <div class="entry">
                <div class="entry-num" style="color: #38BDF8; background: rgba(56, 189, 248, 0.1);">kCFI</div>
                <div class="entry-body">
                    <strong>Control Flow Integrity & BTI:</strong> Blocked all indirect branch hijacking and gadget jumps.
                </div>
            </div>
            <div class="entry">
                <div class="entry-num" style="color: #38BDF8; background: rgba(56, 189, 248, 0.1);">SLUB</div>
                <div class="entry-body">
                    <strong>Slab Isolation & Memory Latency:</strong> Cache line dynamics on physical ARM64 silicon broke heap spray stability.
                </div>
            </div>
        </div>
    </div>

    <div class="bottom-meta">
        <div class="author-sig">Aayush Bankar</div>
        <div class="swipe-action">SWIPE &rarr;</div>
    </div>
</div>

<!-- SLIDE 8: THE REPORT & CTA -->
<div class="slide">
    <div class="top-meta">
        <div class="meta-tag">Dossier Available</div>
        <div class="page-index">08 / 08</div>
    </div>

    <div>
        <h2>The Complete 26-Page Post-Mortem</h2>
        <p class="lead">
            Full technical documentation, verification ledgers, and killing evidence.
        </p>

        <div class="box box-accent">
            <div style="font-size: 16px; font-family: 'SF Mono', monospace; color: #D97706; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 8px;">
                Included in the Whitepaper
            </div>
            <div style="font-size: 21px; line-height: 1.45; color: #EDEDED;">
                • Verification Ledger (VER-001 to VER-039)<br>
                • 21 Documented Dead Ends with GDB traces<br>
                • Full Tier 1 offset tables & ROP gadgets<br>
                • Android GKI kernel compilation harness
            </div>
        </div>

        <div class="box" style="margin-top: 24px; border-color: #3B82F6;">
            <div style="font-size: 24px; font-weight: 700; color: #FFFFFF; margin-bottom: 6px;">
                Want the 26-Page PDF?
            </div>
            <div style="font-size: 21px; color: #93C5FD; line-height: 1.4;">
                Drop <strong style="color: #FBBF24;">"EPOLL"</strong> in the comments and I'll send the direct report straight to your DMs.
            </div>
        </div>
    </div>

    <div class="bottom-meta">
        <div class="author-sig">Aayush Bankar · August 2026</div>
        <div style="color: #D97706; font-weight: 600; font-family: 'SF Mono', monospace; font-size: 15px;">GITHUB / MEDIUM LINKED</div>
    </div>
</div>

</body>
</html>
"""

with open("/mnt/work/company/cyphermatrix/repos/bad-epoll-lab/article/linkedin_carousel_v2.html", "w") as f:
    f.write(html_content)

print("V2 HTML written successfully.")
