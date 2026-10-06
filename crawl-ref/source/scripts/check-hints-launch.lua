-- Run with: ./crawl -hints -script check-hints-launch
local mode = crawl.hints_type()
assert(mode == "berserk" or mode == "magic" or mode == "ranger",
       "The -hints option must start Hints Mode")
crawl.stderr("Direct Hints Mode launch passed.")
