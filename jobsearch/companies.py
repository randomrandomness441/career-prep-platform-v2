"""Target companies for the automated fetch. Edit this list freely -- add a
company, or fix a wrong slug, and re-run fetch_ats.py.

Each entry: (display_name, ats, slug)
  ats is one of: greenhouse | lever | ashby
  slug is the board token/handle in that company's careers URL, e.g.
  jobs.ashbyhq.com/<slug>, jobs.lever.co/<slug>, boards.greenhouse.io/<slug>.

Every row here has been verified live (HTTP 200, real job count checked) --
not guessed. Databricks alone was 841 of 963 fetched postings at one point,
badly skewing what showed up in the UI regardless of any filter, because
most of the other rows had wrong slugs (confluent/hashicorp/snowflake/ramp/
notion all 404'd on the platform originally guessed for them -- several
turned out to be on Ashby, not Greenhouse). Re-verified all of them plus
added real volume from companies not on the list before. HashiCorp isn't
on Greenhouse, Lever, or Ashby under any slug tried -- likely a private/
custom ATS, dropped rather than left broken.
"""

COMPANIES = [
    ("Cockroach Labs", "greenhouse", "cockroachlabs"),
    ("Databricks", "greenhouse", "databricks"),
    ("Confluent", "ashby", "confluent"),
    ("Snowflake", "ashby", "snowflake"),
    ("Airbyte", "ashby", "airbyte"),
    ("Temporal", "ashby", "temporal"),
    ("Linear", "ashby", "linear"),
    ("Ramp", "ashby", "ramp"),
    ("Notion", "ashby", "notion"),
    ("Stripe", "greenhouse", "stripe"),
    ("Airbnb", "greenhouse", "airbnb"),
    ("Pinterest", "greenhouse", "pinterest"),
    ("Reddit", "greenhouse", "reddit"),
    ("Elastic", "greenhouse", "elastic"),
    ("MongoDB", "greenhouse", "mongodb"),
    ("Datadog", "greenhouse", "datadog"),
    ("PlanetScale", "greenhouse", "planetscale"),
    # second verified batch
    ("Figma", "greenhouse", "figma"),
    ("Discord", "greenhouse", "discord"),
    ("Robinhood", "greenhouse", "robinhood"),
    ("Coinbase", "greenhouse", "coinbase"),
    ("Brex", "greenhouse", "brex"),
    ("Webflow", "greenhouse", "webflow"),
    ("Gusto", "greenhouse", "gusto"),
    ("Affirm", "greenhouse", "affirm"),
    ("Chime", "greenhouse", "chime"),
    ("Anthropic", "greenhouse", "anthropic"),
    ("Asana", "greenhouse", "asana"),
    ("Calendly", "greenhouse", "calendly"),
    ("Clerk", "ashby", "clerk"),
    ("Supabase", "ashby", "supabase"),
    ("Amplitude", "ashby", "amplitude"),
    ("Plaid", "ashby", "plaid"),
    ("OpenAI", "ashby", "openai"),
    ("Zapier", "ashby", "zapier"),
    ("Miro", "ashby", "miro"),
    # third verified batch -- sourced from a LinkedIn Bangalore search result
    # Sourav pasted; each slug below was live-checked (HTTP 200, real job
    # count) same as every other row. Microsoft/Nike/Atlassian/Flipkart/
    # Rippling/Aerospike/Navigator/Zamp/Neo/Ambient.ai from that same list
    # are NOT on Greenhouse/Lever/Ashby under any slug tried -- same
    # situation as HashiCorp above, dropped rather than guessed.
    ("Gather AI", "greenhouse", "gatherai"),
    ("H1", "lever", "h1"),
    ("Abnormal AI", "greenhouse", "abnormalsecurity"),
    ("Sigmoid", "greenhouse", "sigmoid"),
    ("Portcast", "lever", "portcast"),
    ("GitLab", "greenhouse", "gitlab"),
    ("Elsevier", "greenhouse", "elsevier"),
    ("Gushwork", "lever", "gushwork"),
    ("YipitData", "greenhouse", "yipitdata"),
    ("Level AI", "lever", "levelai"),
    ("SigNoz", "ashby", "signoz"),
    ("Nightfall AI", "ashby", "nightfall-ai"),
    ("Astra Security", "ashby", "astra"),
    # fourth verified batch -- sourced from real public rankings (Wikipedia's
    # unicorn-by-country list, public SaaS market-cap rankings), not a guess:
    # 96 real, famous/large companies checked against a static 15,862-slug
    # index (Feashliaa/job-board-aggregator, MIT) plus live slug-guessing,
    # then each live-verified to actually have an India/Bangalore posting
    # right now before being added here. 29 more were confirmed reachable
    # but have zero India postings today (Snyk, Mistral AI, Doctolib, Wise,
    # Perplexity, Replit, Checkr, Culture Amp among them) -- not added, since
    # "famous" alone wasn't the bar, a live India posting was. 49 of the 96
    # weren't found on any of the three providers at all (Revolut, Canva,
    # Razorpay, PhonePe, CRED's own listed competitors Dream11/Ola/Swiggy,
    # Atlassian-adjacent Linktree/Go1, etc.) -- same as HashiCorp, likely
    # Workday/custom ATS, dropped rather than guessed.
    ("Scale AI", "greenhouse", "scaleai"),
    ("Harvey", "ashby", "harvey"),
    ("Flexport", "greenhouse", "flexport"),
    ("Fivetran", "greenhouse", "fivetran"),
    ("AlphaSense", "greenhouse", "alphasense"),
    ("Twilio", "greenhouse", "twilio"),
    ("Okta", "greenhouse", "okta"),
    ("CRED", "lever", "cred"),
    ("Meesho", "lever", "meesho"),
    ("Groww", "greenhouse", "groww"),
    ("Celonis", "greenhouse", "celonis"),
    ("ElevenLabs", "ashby", "elevenlabs"),
    ("Airwallex", "ashby", "airwallex"),
    # fifth verified batch -- sourced from Sequoia Capital's own public
    # portfolio (sequoiacap.com/our-companies), extracted from the static
    # Framer search-index JSON their own site serves for client-side search
    # (427 real companies, confirmed live: HTTP 200, no bot-detection) --
    # not a scrape of rendered HTML. 408 of those weren't already tracked;
    # each checked for a live India/Bangalore posting before being
    # considered, same as every other row here. 150 more were confirmed
    # reachable but have zero India postings today; 245 weren't found on
    # any of the three providers.
    #
    # First pass only checked "has an India posting" -- too loose. Re-ran
    # with the real is_likely_engineering_title() check against each live
    # posting and dropped every company whose only India postings turned
    # out to be non-engineering once actually read: Agency (833 postings,
    # all "Freelance AI Trainer Project" gig-marketplace work, likely a
    # slug collision on a generic word besides), NUVACORE (real engineering
    # jobs, but hardware/silicon verification -- a different discipline
    # than the software/distributed-systems target, correctly out of
    # scope), Chainguard/Cresta (Account Executive, Sales Engineer, Demo
    # Engineer -- all sales-adjacent), Meter/Domino Data Lab (Support
    # Engineer, IT Support Engineer -- already-denylisted support roles).
    ("UiPath", "ashby", "uipath"),
    ("Sumo Logic", "greenhouse", "sumologic"),
    ("LinkedIn", "lever", "linkedin"),
    ("Aspora", "ashby", "aspora"),
    ("Waymo", "greenhouse", "waymo"),
    ("Handshake", "ashby", "handshake"),
    ("Commure", "ashby", "commure"),
    # sixth verified batch -- sourced from YC's own public "currently
    # hiring" directory (yc-oss/api, a daily-refreshed open mirror of YC's
    # Algolia-backed company directory, not a scrape -- see the fetch
    # script's docstring). 1,486 hiring companies, 1,476 not already
    # tracked; same India+engineering re-verification as the Sequoia batch
    # above from the start this time. 424 more confirmed reachable with no
    # India postings today; 1,029 weren't found on any of the three
    # providers (mostly very early-stage YC companies with no formal ATS
    # yet). 9 India-posting matches were found and DROPPED after the real
    # per-posting title check: Xendit, Canary Technologies, Alpaca, Encord,
    # Sieve, Fleek, LiteLLM (India postings exist but none are engineering),
    # Aleph (entirely ads-sales/finance/recruiting in India), Agency (same
    # gig-marketplace board already found via Sequoia).
    ("HackerRank", "greenhouse", "hackerrank"),
    ("Instawork", "greenhouse", "instawork"),
    ("Netomi", "lever", "netomi"),
    ("Apollo.io", "greenhouse", "apolloio"),
    ("Prodigal", "greenhouse", "prodigal"),
    ("Overview", "ashby", "overview"),
    ("FamPay", "lever", "fampay"),
    ("Triomics", "ashby", "triomics"),
    ("AiPrise", "ashby", "aiprise"),
    ("Avoca", "ashby", "avoca"),
    ("FurtherAI", "ashby", "furtherai"),
    ("FirstWork", "ashby", "firstwork"),
    ("Mesh", "greenhouse", "mesh"),
    ("Cardboard", "ashby", "cardboard"),
    # seventh verified batch -- sourced from two US VC funds' own public
    # portfolio pages, both confirmed live (HTTP 200, real company slugs
    # directly in the raw HTML, no bot-detection): Greylock
    # (greylock.com/portfolio/, 168 companies) and NEA
    # (nea.com/portfolio, 918 companies -- many defunct/legacy entries from
    # decades of investing, expected). Real India+engineering check applied
    # from the start this time (the lesson from the Sequoia/YC batches
    # above): 154 Greylock and 911 NEA candidates weren't already tracked;
    # only these 13 had an actual live engineering posting in India, not
    # just an India office of any kind. A systematic sweep of the rest of
    # the major US fund list (a16z, Bessemer, Insight Partners, Khosla, GV,
    # Founders Fund, Index Ventures, Kleiner Perkins, Tiger Global,
    # Benchmark, Thrive Capital, General Catalyst) found no comparable
    # static data source -- all either client-render their portfolio page
    # with no data in the initial HTML, have no portfolio page at all
    # (Benchmark), explicitly disallow it in robots.txt (General Catalyst),
    # gate their data API behind a paid key (Thrive, via the Getro
    # platform), or actively block non-browser requests (Lightspeed,
    # Battery -- real Cloudflare challenge pages, same category already
    # declined for company career sites earlier).
    ("Oportun", "greenhouse", "oportun"),
    ("Pure Storage", "greenhouse", "purestorage"),
    ("Rubrik", "greenhouse", "rubrik"),
    ("AI Squared", "greenhouse", "aisquared"),
    ("Appian", "greenhouse", "appian"),
    ("Bloomreach", "greenhouse", "bloomreach"),
    ("Conviva", "greenhouse", "conviva"),
    ("Coursera", "greenhouse", "coursera"),
    ("MoonPay", "lever", "moonpay"),
    ("Reltio", "greenhouse", "reltio"),
    ("ScienceLogic", "ashby", "sciencelogic"),
    ("Scopely", "greenhouse", "scopely"),
    ("Together AI", "greenhouse", "togetherai"),
]
