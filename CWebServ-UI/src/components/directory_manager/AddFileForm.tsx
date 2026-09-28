import { useContext, useState } from "react"
import { FileTreeContext } from "../../providers/FileTreeProvider"
import { Alert, Button, Form, FormControl, FormGroup, FormLabel, Modal, ModalBody, ModalDialog, ModalFooter, ModalHeader } from "react-bootstrap"
import { postDirectory, postFile } from "../../utils/RequestUtils"
import { CSRFContext } from "../../providers/CSRFProvider"


export default function addFileForm({ filePath, show, setHide }: { filePath: string, show: boolean, setHide: () => void }) {
    const { addFile } = useContext(FileTreeContext)
    const csrf = useContext(CSRFContext)

    let [file, setFile] = useState(null)
    let [fileName, setFileName] = useState("")
    let [isDir, setIsDir] = useState(false)

    let [error, setError] = useState("")

    return <Modal show={show}><ModalDialog >
        <ModalHeader>
            {error && <Alert variant="danger">{error}</Alert>}
        </ModalHeader>
        <ModalBody>
            <Form>
                <FormGroup>
                    <FormLabel htmlFor="fileInp"></FormLabel>
                    <FormControl id="fileName" type="text" onChange={(e) => {
                        e.stopPropagation()
                        setFileName(e.target.value)
                    }} value={fileName} />
                </FormGroup>
                <FormGroup>
                    <FormLabel htmlFor="fileInp"></FormLabel>
                    <FormControl id="fileInp" type="file" onChange={(e) => {
                        e.stopPropagation()
                        setFile(e.currentTarget.files[0])
                    }} />
                </FormGroup>
                <FormGroup>
                    <FormLabel htmlFor="isDirInp"></FormLabel>
                    <FormControl id="isDirInp" type="checkbox" onChange={(e) => {
                        e.stopPropagation()
                        setIsDir(e.target.checked)
                    }} checked={isDir} />
                </FormGroup>
            </Form>
        </ModalBody>
        <ModalFooter>
            <Button type="button" onClick={async (e) => {
                e.stopPropagation()
                if (!fileName) {
                    setError("Please Enter a filename!")
                    return
                }
                if (!file && !isDir) {
                    setError("Please Enter a file!")
                    return

                }

                if (isDir){
                    await postDirectory(filePath,fileName, csrf)
                }else{
                }
                const reader = new FileReader()
                reader.onload = async (e) => {
                    const data = e.target.result as ArrayBuffer
                    await postFile(filePath, fileName, data, csrf)
                };
                reader.readAsArrayBuffer(file)

            }} variant="success">Add File</Button>
            <Button type="button" onClick={(e) => {
                e.stopPropagation()
                setHide()
            }} variant="success">Cancel</Button>
        </ModalFooter>
    </ModalDialog></Modal>

}
