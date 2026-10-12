"""Where a species' art lives in SpriteCollab, in order of preference.

ONE list, read by pack_pmd.py (which fetches the art) and check_sprites.py
(which decides whether a species has art, and so whether it may hatch). They
used to look only at sprite/NNNN/AnimData.xml, so a species drawn only under a
gender or form folder counted as having no art at all:

  668 PYROAR        drawn only as the female       0000/0000/0002
  741 ORICORIO      Baile undrawn; its other styles 0001..0003
  870 FALINKS       only forms 0002/0003
  999 GIMMIGHOUL    only the Roaming form          0001
  1008 MIRAIDON     only an alternate mode         0001

Each is still that species, so drawing it beats a bare dex number. The first
entry that exists wins, so a species WITH default art is unaffected.
SpriteCollab's layout is sprite/<dex>/<form>/<shiny>/<gender>.
"""

# Sub-paths under sprite/<dex>, best first. '' is the default sprite.
ART_PATHS = ['', '/0000/0000/0002', '/0001', '/0002', '/0003']


def shiny_path(sub):
    """The shiny variant of an art sub-path ('' -> '/0000/0001')."""
    if sub == '':
        return '/0000/0001'
    if sub == '/0000/0000/0002':
        return '/0000/0001/0002'
    return sub + '/0001'
