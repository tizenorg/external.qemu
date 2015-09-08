#!/bin/bash -e
#
# While updating versions of QEMU to 1.0 I got fed up with the
# quilt workflow and just put up a git tree that contains all
# the commits on top of a stable tarball.
#
# When updating this package, just either update the git tree
# below (use rebase!) or change the tree path and use your own
#
# That way we can easily rebase against the next stable release
# when it comes.

GIT_TREE=git://repo.or.cz/qemu/agraf.git
GIT_LOCAL_TREE=/suse/agraf/git/qemu
GIT_BRANCH=suse-1.2
GIT_UPSTREAM_TAG=v1.2.0
QEMU_TMP=/dev/shm/qemu-tmp

# clean up
if [ -e 0001-* ]; then
    osc rm --force 0*
fi
rm -f qemu.spec

# fetch all patches
rm -rf $QEMU_TMP
OSCDIR="$(pwd)"
if [ -d "$GIT_LOCAL_TREE" ]; then
    git clone -ls $GIT_LOCAL_TREE $QEMU_TMP
    cd $QEMU_TMP
else
    git clone $GIT_TREE $QEMU_TMP
    cd $QEMU_TMP
    git remote add upstream git://git.qemu.org/qemu.git
    git remote update
fi
git checkout $GIT_BRANCH 
git format-patch -N $GIT_UPSTREAM_TAG -o "$OSCDIR"
cd "$OSCDIR"
rm -rf $QEMU_TMP

# cut off file name after 40 bytes, so we work around git version differences
# while at it, also remove the signature
for i in 0*; do
    PATCHNAME=${i%.patch}
    PATCHNAME=${i:0:40}.patch
    head -n $(expr $(wc -l $i | cut -d ' ' -f 1) - 3) $i > "$PATCHNAME.tmp"
    rm "$i"
    mv "$PATCHNAME.tmp" "$PATCHNAME"
done

# we have all patches as files now - generate the spec file!
while IFS= read -r line; do
    if [ "$line" = "PATCH_FILES" ]; then
        for i in 0*; do
            NUM=${i%%-*}
            echo -e "Patch$NUM:      $i"
        done
    elif [ "$line" = "PATCH_EXEC" ]; then
        for i in 0*; do
            NUM=${i%%-*}
            echo "%patch$NUM -p1"
        done
    else
        echo "$line"
    fi
done < qemu.spec.in > qemu.spec
osc add 0*

