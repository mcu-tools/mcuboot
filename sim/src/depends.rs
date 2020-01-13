// Copyright (c) 2019 Linaro LTD
//
// SPDX-License-Identifier: Apache-2.0

//! Support and tests related to the dependency management for multi-image
//! support.

use crate::image::ImageVersion;

pub trait Depender {
    /// Generate a version for this particular image.  The slot indicates
    /// which slot this is being put in.
    fn my_version(&self, offset: usize, slot: usize) -> ImageVersion;

    /// Return dependencies for this image/slot combination.
    fn my_deps(&self, offset: usize, slot: usize) -> Vec<ImageVersion>;

    /// Return the image ID of the other version.
    fn other_id(&self) -> u8;
}

/// A boring image is used when we aren't testing dependencies.  There will
/// be meaningful version numbers.  The size field is the image number we
/// are.
pub struct BoringDep {
    number: usize,
    test: DepTest,
}

impl BoringDep {
    pub fn new(number: usize, test: &DepTest) -> BoringDep {
        BoringDep {
            number: number,
            test: test.clone(),
        }
    }
}

impl Depender for BoringDep {
    fn my_version(&self, _offset: usize, slot: usize) -> ImageVersion {
        let slot = if self.test.downgrade {
            1 - slot
        } else {
            slot
        };
        ImageVersion::new_synthetic(self.number as u8, slot as u8, 0)
    }

    fn my_deps(&self, _offset: usize, _slot: usize) -> Vec<ImageVersion> {
        vec![]
    }

    fn other_id(&self) -> u8 {
        0
    }
}

/// An individual test of the dependency mechanism describes one of the
/// possibilities for the dependency information for each image, and what
/// the expected outcome is.
#[derive(Clone, Debug)]
pub struct DepTest {
    /// What kinds of dependency should be installed in the image.
    pub depends: [DepType; 2],

    /// What is the expected outcome of the upgrade.
    pub upgrades: [UpgradeInfo; 2],

    /// Should this be considered a downgrade (cause the version number to
    /// decrease).
    pub downgrade: bool,
}

/// Describes the various types of dependency information that can be
/// provided.
#[derive(Clone, Debug)]
pub enum DepType {
    /// Do not include dependency information
    Nothing,
    /// Provide dependency information that matches the other image.
    Correct,
    /// Provide a dependency that matches the old version of the other
    /// image.
    OldCorrect,
    /// Provide dependency information describing something newer than the
    /// other image.
    Newer,
    /// Don't provide an upgrade image at all for this image
    NoUpgrade,
}

/// Describes what our expectation is for an upgrade.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum UpgradeInfo {
    /// The current version should be held.
    Held,
    /// The image should be upgraded
    Upgraded,
}

/// A "test" that gives no dependency information.
pub static NO_DEPS: DepTest = DepTest {
    depends: [DepType::Nothing, DepType::Nothing],
    upgrades: [UpgradeInfo::Upgraded, UpgradeInfo::Upgraded],
    downgrade: false,
};

/// A "test" with no dependency information, and the images marked as a
/// downgrade.
pub static REV_DEPS: DepTest = DepTest {
    depends: [DepType::Nothing, DepType::Nothing],
    upgrades: [UpgradeInfo::Held, UpgradeInfo::Held],
    downgrade: true,
};

/// A PairDep describes the dependencies between two pairs.
pub struct PairDep {
    /// The image number of this image.
    number: usize,

    test: DepTest,
}

impl PairDep {
    pub fn new(total_image: usize, my_image: usize, deps: &DepTest) -> PairDep {
        if total_image != 2 {
            panic!("PairDep only works when there are two images");
        }

        PairDep {
            number: my_image,
            test: deps.clone(),
        }
    }
}

impl Depender for PairDep {
    fn my_version(&self, _offset: usize, slot: usize) -> ImageVersion {
        let slot = if self.test.downgrade {
            1 - slot
        } else {
            slot
        };
        ImageVersion::new_synthetic(self.number as u8, slot as u8, 0)
    }

    fn my_deps(&self, _offset: usize, slot: usize) -> Vec<ImageVersion> {
        // For now, don't put any dependencies in slot zero.  They could be
        // added here if we someday implement checking these.
        if slot == 0 {
            vec![]
        } else {
            match self.test.depends[self.number] {
                DepType::Nothing => vec![],
                DepType::NoUpgrade => panic!("Shouldn't get to this point"),
                DepType::Correct => vec![
                    ImageVersion::new_synthetic(self.other_id(), slot as u8, 0)
                ],
                DepType::OldCorrect => vec![
                    ImageVersion::new_synthetic(self.other_id(), 0, 0)
                ],
                DepType::Newer => vec![
                    ImageVersion::new_synthetic(self.other_id(), slot as u8, 1)
                ],
            }
        }
    }

    fn other_id(&self) -> u8 {
        (1 - self.number) as u8
    }
}

impl ImageVersion {
    /// Generate an image version based on some key information.  The image
    /// number influences the major version number (by an arbitrary factor),
    /// and the slot affects the major number on the build_number.  The minor
    /// number can also be given to force numbers to be different.
    fn new_synthetic(image_id: u8, slot: u8, minor: u8) -> ImageVersion {
        ImageVersion {
            major: image_id * 20 + slot,
            minor: minor,
            revision: 1,
            build_num: slot as u32,
        }
    }
}
