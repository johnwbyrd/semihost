Documentation Sources
=====================

The Zero Board Computer project has two documentation surfaces, and
they hold different material on purpose.  This page is the canonical
statement of which surface holds what, who edits it, and how changes
flow.

Reference (this site)
---------------------

The Sphinx site you are reading.  Holds **normative** material -- the
content that implementations and conforming users must trust to be
correct.

- **What it holds:** the protocol specification, the API reference for
  the C and C++ libraries, operational guides (building, testing,
  security), worked examples, and proposals for future extensions.
- **Who edits it:** project maintainers, via pull requests against
  this repository.
- **Where it renders:** https://johnwbyrd.github.io/zbc/ (a move to
  ``docs.zeroboardcomputer.com`` is planned).
- **How changes flow:** edit ``docs/source/*.rst``, push to ``main``,
  the ``docs.yml`` workflow rebuilds the site and redeploys
  automatically.

Wiki (zeroboardcomputer.com)
----------------------------

The MediaWiki site at https://www.zeroboardcomputer.com.  Holds
**descriptive** material -- the content that captures what users
observe, accomplish, and learn from each other.

- **What it holds:** tutorials, how-to guides, troubleshooting
  collections, CPU and toolchain compatibility tables, ecosystem links
  (related projects, ports, forks), war stories, and the MediaWiki
  templates and navigation that the wiki itself uses.
- **Who edits it:** anyone with a wiki account.
- **Where it renders:** https://www.zeroboardcomputer.com.
- **How changes flow:** log in, click *Edit*, save.  No git, no review
  queue, no pull request.

When in doubt
-------------

Use this rubric to decide where a piece of content belongs:

- Does the content claim to be **normative** -- correct, authoritative,
  binding on conforming implementations?  → Reference.
- Does it describe what a user **observed**, accomplished, or learned
  the hard way?  → Wiki.
- Could it be **wrong** without breaking a conforming implementation?
  → Wiki.
- Is it a **tutorial**, a walkthrough, or a "how I got X working"
  story?  → Wiki.

A simpler version: if it has to be right for the protocol to work, put
it in the Reference.  Otherwise, the Wiki.

Regenerating wiki content
-------------------------

The wiki is authored directly at https://www.zeroboardcomputer.com.

There is no draft area in this repository.  There is no
``web/pages/`` mirror that gets synced.  When the wiki needs new
content -- tutorials, how-tos, compatibility tables, ecosystem entries
-- it gets written on the wiki, edited on the wiki, and lives on the
wiki.  The two doc surfaces hold *different* material; the wiki is not
regenerated from the spec, and the spec is not extracted from the
wiki.

The ``web/`` directory in this repository contains historical content
from an earlier model in which the wiki was treated as derived
documentation regenerated from the spec.  That model is superseded by
this page.  Retiring the legacy content under ``web/`` is tracked
separately; in the meantime, treat anything under ``web/`` as legacy,
not as the current source of truth for wiki content.  **The current
source of truth for wiki content is the wiki.**
