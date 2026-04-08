use ratatui::prelude::Rect;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SplitDir {
    Horizontal,
    Vertical,
}

#[derive(Debug)]
pub enum LayoutNode {
    Leaf(i32),
    Split {
        dir: SplitDir,
        ratio: f32,
        children: Box<(LayoutNode, LayoutNode)>,
    },
}

impl LayoutNode {
    pub fn leaf(buffer_id: i32) -> Self {
        LayoutNode::Leaf(buffer_id)
    }

    pub fn split(dir: SplitDir, first: LayoutNode, second: LayoutNode) -> Self {
        LayoutNode::Split {
            dir,
            ratio: 0.5,
            children: Box::new((first, second)),
        }
    }

    pub fn layout(&self, area: Rect) -> Vec<(i32, Rect)> {
        let mut result = Vec::new();
        self.layout_inner(area, &mut result);
        result
    }

    fn layout_inner(&self, area: Rect, out: &mut Vec<(i32, Rect)>) {
        match self {
            LayoutNode::Leaf(id) => {
                out.push((*id, area));
            }
            LayoutNode::Split { dir, ratio, children } => {
                let (first_area, second_area) = match dir {
                    SplitDir::Horizontal => {
                        let split = (area.height as f32 * ratio) as u16;
                        (
                            Rect::new(area.x, area.y, area.width, split),
                            Rect::new(area.x, area.y + split, area.width, area.height - split),
                        )
                    }
                    SplitDir::Vertical => {
                        let split = (area.width as f32 * ratio) as u16;
                        (
                            Rect::new(area.x, area.y, split, area.height),
                            Rect::new(area.x + split, area.y, area.width - split, area.height),
                        )
                    }
                };
                children.0.layout_inner(first_area, out);
                children.1.layout_inner(second_area, out);
            }
        }
    }

    pub fn contains(&self, buffer_id: i32) -> bool {
        match self {
            LayoutNode::Leaf(id) => *id == buffer_id,
            LayoutNode::Split { children, .. } => {
                children.0.contains(buffer_id) || children.1.contains(buffer_id)
            }
        }
    }

    pub fn leaf_count(&self) -> usize {
        match self {
            LayoutNode::Leaf(_) => 1,
            LayoutNode::Split { children, .. } => {
                children.0.leaf_count() + children.1.leaf_count()
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn single_leaf_fills_area() {
        let tree = LayoutNode::leaf(1);
        let area = Rect::new(0, 0, 100, 50);
        let rects = tree.layout(area);
        assert_eq!(rects.len(), 1);
        assert_eq!(rects[0], (1, area));
    }

    #[test]
    fn vertical_split_divides_width() {
        let tree = LayoutNode::split(
            SplitDir::Vertical,
            LayoutNode::leaf(1),
            LayoutNode::leaf(2),
        );
        let area = Rect::new(0, 0, 100, 50);
        let rects = tree.layout(area);
        assert_eq!(rects.len(), 2);
        assert_eq!(rects[0], (1, Rect::new(0, 0, 50, 50)));
        assert_eq!(rects[1], (2, Rect::new(50, 0, 50, 50)));
    }

    #[test]
    fn horizontal_split_divides_height() {
        let tree = LayoutNode::split(
            SplitDir::Horizontal,
            LayoutNode::leaf(1),
            LayoutNode::leaf(2),
        );
        let area = Rect::new(0, 0, 100, 50);
        let rects = tree.layout(area);
        assert_eq!(rects.len(), 2);
        assert_eq!(rects[0], (1, Rect::new(0, 0, 100, 25)));
        assert_eq!(rects[1], (2, Rect::new(0, 25, 100, 25)));
    }

    #[test]
    fn nested_splits() {
        let tree = LayoutNode::split(
            SplitDir::Vertical,
            LayoutNode::leaf(1),
            LayoutNode::split(
                SplitDir::Horizontal,
                LayoutNode::leaf(2),
                LayoutNode::leaf(3),
            ),
        );
        let rects = tree.layout(Rect::new(0, 0, 100, 100));
        assert_eq!(rects.len(), 3);
        assert_eq!(rects[0].0, 1);
        assert_eq!(rects[1].0, 2);
        assert_eq!(rects[2].0, 3);
    }

    #[test]
    fn contains_finds_buffer() {
        let tree = LayoutNode::split(
            SplitDir::Vertical,
            LayoutNode::leaf(1),
            LayoutNode::leaf(2),
        );
        assert!(tree.contains(1));
        assert!(tree.contains(2));
        assert!(!tree.contains(3));
    }

    #[test]
    fn leaf_count() {
        let tree = LayoutNode::split(
            SplitDir::Vertical,
            LayoutNode::leaf(1),
            LayoutNode::split(
                SplitDir::Horizontal,
                LayoutNode::leaf(2),
                LayoutNode::leaf(3),
            ),
        );
        assert_eq!(tree.leaf_count(), 3);
    }
}
