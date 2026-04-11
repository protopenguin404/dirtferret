use crate::region::{Anchor, Region};

/// Compute positions and sizes for all regions given the terminal dimensions.
///
/// Assigns `cell_row`, `pixel_y`, `pixel_width`, and `pixel_height` to each
/// region based on its anchor rule. Calls `region.resize()` only if the
/// computed dimensions differ from the current ones.
///
/// Algorithm:
/// 1. Top-anchored regions consume rows from the top downward
/// 2. Bottom-anchored regions consume rows from the bottom upward
/// 3. Fill regions divide the remaining space evenly
pub fn compute_layout(
    regions: &mut [&mut Region],
    pixel_width: u32,
    pixel_height: u32,
    cell_cols: u16,
    cell_rows: u16,
) -> anyhow::Result<()> {
    let cell_h = if cell_rows > 0 {
        pixel_height / cell_rows as u32
    } else {
        16
    };

    // Track consumed rows from top and bottom.
    let mut top_row: u16 = 1;
    let mut bottom_available: u16 = cell_rows;

    // First pass: Top-anchored regions, in order.
    for region in regions.iter_mut() {
        if let Anchor::Top { height_cells } = region.anchor {
            region.cell_row = top_row;
            region.pixel_y = (top_row as u32 - 1) * cell_h;
            region.cell_cols = cell_cols;
            region.cell_rows_span = height_cells;

            let region_pixel_height = height_cells as u32 * cell_h;
            region.resize(pixel_width, region_pixel_height)?;

            top_row += height_cells;
        }
    }

    // Second pass: Bottom-anchored regions, in order.
    for region in regions.iter_mut() {
        if let Anchor::Bottom { height_cells } = region.anchor {
            bottom_available -= height_cells;
            region.cell_row = bottom_available + 1;
            region.pixel_y = bottom_available as u32 * cell_h;
            region.cell_cols = cell_cols;
            region.cell_rows_span = height_cells;

            let region_pixel_height = height_cells as u32 * cell_h;
            region.resize(pixel_width, region_pixel_height)?;
        }
    }

    // Third pass: Fill regions divide remaining space.
    let fill_count: usize = regions.iter()
        .filter(|r| matches!(r.anchor, Anchor::Fill))
        .count();

    if fill_count > 0 {
        let remaining_cells = bottom_available.saturating_sub(top_row).saturating_add(1);
        let base_cells = remaining_cells / fill_count as u16;
        let remainder = remaining_cells % fill_count as u16;

        let mut current_row = top_row;
        let mut fill_index: u16 = 0;

        for region in regions.iter_mut() {
            if matches!(region.anchor, Anchor::Fill) {
                // Last fill region gets the remainder cells.
                let cells = if fill_index == fill_count as u16 - 1 {
                    base_cells + remainder
                } else {
                    base_cells
                };

                region.cell_row = current_row;
                region.pixel_y = (current_row as u32 - 1) * cell_h;
                region.cell_cols = cell_cols;
                region.cell_rows_span = cells;

                let region_pixel_height = cells as u32 * cell_h;
                region.resize(pixel_width, region_pixel_height)?;

                current_row += cells;
                fill_index += 1;
            }
        }
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::region::Region;

    #[test]
    fn test_basic_layout() {
        // 3 regions: tab bar (Top/1), viewport (Fill), status (Bottom/1)
        // Terminal: 1920x1080 pixels, 54 rows
        let mut tab_bar = Region::new(200, Anchor::Top { height_cells: 1 }).unwrap();
        let mut viewport = Region::new(201, Anchor::Fill).unwrap();
        let mut status = Region::new(202, Anchor::Bottom { height_cells: 1 }).unwrap();

        let mut refs: Vec<&mut Region> = vec![&mut tab_bar, &mut viewport, &mut status];
        compute_layout(&mut refs, 1920, 1080, 240, 54).unwrap();

        // cell_h = 1080 / 54 = 20
        // Tab bar: row 1, pixel_y = 0, height = 1 * 20 = 20
        assert_eq!(tab_bar.cell_row, 1);
        assert_eq!(tab_bar.pixel_y, 0);
        assert_eq!(tab_bar.pixel_width, 1920);
        assert_eq!(tab_bar.pixel_height, 20);

        // Viewport: row 2, pixel_y = 20, height = 52 * 20 = 1040
        assert_eq!(viewport.cell_row, 2);
        assert_eq!(viewport.pixel_y, 20);
        assert_eq!(viewport.pixel_width, 1920);
        assert_eq!(viewport.pixel_height, 1040);

        // Status: row 54, pixel_y = 53 * 20 = 1060, height = 1 * 20 = 20
        assert_eq!(status.cell_row, 54);
        assert_eq!(status.pixel_y, 1060);
        assert_eq!(status.pixel_width, 1920);
        assert_eq!(status.pixel_height, 20);
    }

    #[test]
    fn test_two_fills() {
        // 2 Fill regions, terminal of 100 rows, 1600x1600 pixels
        let mut fill_a = Region::new(210, Anchor::Fill).unwrap();
        let mut fill_b = Region::new(211, Anchor::Fill).unwrap();

        let mut refs: Vec<&mut Region> = vec![&mut fill_a, &mut fill_b];
        compute_layout(&mut refs, 1600, 1600, 200, 100).unwrap();

        // cell_h = 1600 / 100 = 16
        // Each fill gets 50 rows
        assert_eq!(fill_a.cell_row, 1);
        assert_eq!(fill_a.pixel_y, 0);
        assert_eq!(fill_a.pixel_width, 1600);
        assert_eq!(fill_a.pixel_height, 800); // 50 * 16

        assert_eq!(fill_b.cell_row, 51);
        assert_eq!(fill_b.pixel_y, 800);
        assert_eq!(fill_b.pixel_width, 1600);
        assert_eq!(fill_b.pixel_height, 800);
    }
}
