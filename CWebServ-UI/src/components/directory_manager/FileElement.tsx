import { useContext, useState } from "react"
import type { FileData } from "../../utils/RequestUtils"
import FileTypeIcon from "./FileTypeIcon"
import { FileTreeContext } from "../../providers/FileTreeProvider"
import { Button, Collapse, Stack } from "react-bootstrap"
import RemoveFileButton from "./RemoveFileButton"


export interface UIFile extends FileData {
    full_path: string
    children: string[]
};

export default function FileElement({ path }: { path: string }) {

    const { file_tree } = useContext(FileTreeContext)
    const file = file_tree[path]

    let [childrenShow, setChildrenShow] = useState<boolean>(false)

    return <Stack>
        <FileTypeIcon mimetype={file.mimetype} />
        <span>{file.file_name}</span>
        <span>{new Date(file.mod_time).toLocaleTimeString()}</span>
        <Button onClick={(e) => { setChildrenShow(!childrenShow) }}>Show children</Button>
        <RemoveFileButton filePath={path} />
        {childrenShow && file.children &&
            <Collapse in={childrenShow}>
                <Stack>
                    {file.children.map((child) => <FileElement path={`${path}/${child}`} />)}
                </Stack>
            </Collapse>}
    </Stack>
}
